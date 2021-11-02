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
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _inflight_bytes; }

void TCPSender::fill_window() {
    // don't consider the ackno temporarily 
    if (_fin_sent || (_window_size == 0 && _syn_sent) 
                  || (_syn_sent && _stream.buffer_empty() && !_stream.input_ended())){   // no window for transmitting or fin have be sent
        return ;
    }
    if(! _syn_sent){           // build a connection, firstly, send syn flag and client isn.
        _syn_sent = true;
        TCPSegment segment{};
        segment.header().syn = true;
        segment.header().seqno = wrap(_next_seqno,_isn);
        _segments_out.push(segment);
        _inflight_segments.push(segment);
        _next_seqno += segment.length_in_sequence_space();
        _inflight_bytes += segment.length_in_sequence_space();
    }
    else{
        int segment_data_len = min(TCPConfig::MAX_PAYLOAD_SIZE,_window_size);   
        // stream.read() will know if this length is over the buffer length 
        TCPSegment segment{};
        segment.header().seqno = wrap(_next_seqno,_isn);
        segment.payload() =  _stream.read(segment_data_len);
        size_t segment_length = segment.length_in_sequence_space();
        _next_seqno += segment_length;

        _window_size -= segment_length;
        if(_stream.eof() && _window_size>0){
            segment.header().fin = true;
            _fin_sent = true;
            _window_size -= 1;
        }
        _inflight_segments.push(segment);
        _segments_out.push(segment);
        _inflight_bytes += segment_length;
    }
    fill_window(); // 递归的思想
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_index_no = unwrap(ackno,_isn,_next_seqno);
    if (absolute_index_no < _inflight_begin_seqno || absolute_index_no > _next_seqno){
        return ;
    }
    _window_size = window_size;
    _inflight_bytes -= (absolute_index_no - _inflight_begin_seqno);
    _inflight_begin_seqno = absolute_index_no;
    while(true){
        // if (_inflight_segments.front().length_in_sequence_space() > 0
        WrappingInt32 segment_num = _inflight_segments.front().header().seqno;
        uint64_t segment_index = unwrap(segment_num,_isn,_next_seqno);
        if(segment_index < absolute_index_no){
            _inflight_segments.pop();
        }
        else{
            break;
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _now_timer += ms_since_last_tick;
    if (_now_timer > _initial_retransmission_timeout){
        // retransmit segment
        // double the retransmission timeout when time out
        _initial_retransmission_timeout = _initial_retransmission_timeout * 2;
        _now_timer = 0;
        if(!_inflight_segments.empty()){
            TCPSegment temp = _inflight_segments.front();
            _segments_out.push(temp);
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return 1; }

void TCPSender::send_empty_segment() {
    TCPSegment segment{};
    segment.header().seqno = wrap(_next_seqno,_isn);
    _segments_out.push(segment);
}
