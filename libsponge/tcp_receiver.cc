#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader rec_header = seg.header();
    if (rec_header.syn && !_syn_recvd){
            _syn_recvd = true;
            _isn = rec_header.seqno;
    }
    if(rec_header.fin){
        if(!_fin_recvd && _syn_recvd){
            _fin_recvd = true;
        }
        else{
            return ;
        }
    }
    if (!rec_header.fin && !rec_header.syn && !_syn_recvd){
        return ;
    }
    string data = seg.payload().copy();
    size_t data_index = unwrap(rec_header.seqno,_isn,_index);
    // uint64_t temp_num = _index - _reassembler.stream_out().bytes_written();
    if(rec_header.syn){
        data_index = 1;
    }
    _reassembler.push_substring(data,data_index - 1 ,rec_header.fin);
    _index =  _reassembler.stream_out().bytes_written(); // + (rec_header.fin? 1:0)
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_syn_recvd){
        return optional<WrappingInt32>{};
    }
    if(_fin_recvd && _reassembler.stream_out().input_ended()){
        WrappingInt32 result = wrap(_index+2,_isn);
        return result;
    }
    
    WrappingInt32 result = wrap(_index+1,_isn);
    return result;
}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.stream_out().buffer_size();
 }
