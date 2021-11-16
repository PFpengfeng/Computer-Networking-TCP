#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _byte_in_flight;
}

void TCPSender::fill_window() {
    TCPSegment seg; //本次方法要发送的段
    //syn包是否已经发送
    if(!_syn){
        //如果还未发送,进行包装
        seg.header().syn = true;
        seg.header().seqno = wrap(0,_isn);
        //更新值与状态
        _syn = true;
        _next_seqno = 1;
        _segments_outstanding.push(seg);
        _byte_in_flight += 1;
        //发送
        _segments_out.push(seg);
    }else{
        //发送其他段
        uint64_t window_size = (_win_size == 0 ? 1 : _win_size);
        uint64_t remain_size{};
        /// when window isn't full and never sent FIN
        while(!_fin && (remain_size = window_size - (_next_seqno - _recv_ackno)) != 0){
            size_t payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE,remain_size);
            string str = _stream.read(payload_size);
            seg.payload() = Buffer(std::move(str));
            // fin段也可以携带数据
            if(_stream.eof() && seg.length_in_sequence_space() < remain_size){
                seg.header().fin = true;
                _fin = true;
            }
            
            //stream is empty, break
            if(seg.length_in_sequence_space() == 0){
                break;
            }
            // stream不为空，则封装TCPSegment，然后发送，更新状态信息
            seg.header().seqno = next_seqno();
            _next_seqno += seg.length_in_sequence_space();
            _byte_in_flight += seg.length_in_sequence_space();
            _segments_out.push(seg);
            _segments_outstanding.push(seg);
        }
    }
    // 每次发送段时，如果timer未启动，则启动
    if(!_time_run){
        _time_run = true;
        _timer = 0;
    }  
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t abs_ackno = unwrap(ackno,_isn,_recv_ackno);
    _win_size = window_size;
    //说明改ackno之前的段已经都确认过了，直接返回
    if(abs_ackno <= _recv_ackno) return;
    //否则更新_recv_ackno 的值为当前ackno
    _recv_ackno = abs_ackno;
    TCPSegment seg;
    //剔除掉ackno之前已经全部确认的段
    while(!_segments_outstanding.empty()){
        seg = _segments_outstanding.front(); // lab采用的是回退N步
        //判断取得的头段，其序号值的范围是否小于 abs_ackno
        if(unwrap(seg.header().seqno,_isn,_recv_ackno) + seg.length_in_sequence_space() <= abs_ackno){
            //是则说明该段不需要重发,可以从重发queue中删掉了
            //更新状态值
            _byte_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
        }else{
            break;
        }
    }
    //继续调用填充窗口函数，看看是否能继续发送
    fill_window();
    //Lab讲到，当对等端的接受方发送ackno 成功确认报文段时，就需要重置RTO的值为初始值1s，重启_timer，以便其他报文段能使用超时重传计时器，重置连续重传次数
    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;
    _timer = 0;
    return;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    //_timer每次加上ms_since_last_tick（即自上次tick()被调用经过的时间）
    _timer += ms_since_last_tick;
    //判断 _timer 是否已经超过我们设定的 _retransmission_timeout 即 1s（刚开始是 1s）
    if(_timer >= _retransmission_timeout && _time_run && !_segments_outstanding.empty()){
        //超时则重传
        _segments_out.push(_segments_outstanding.front());
        //重传次数+1
        _consecutive_retransmissions ++;
        //如果此时 tick 被调用 && 超时 && 窗口大小不为0，则认为是网络阻塞，指数规避，延长重传时间
        if(_segments_outstanding.front().header().syn == true || _win_size != 0){
            _retransmission_timeout *= 2;
        }
        // 将 _timer 重新置0，以重新计算到达下一次超时的时间
        _timer = 0;
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions;}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
