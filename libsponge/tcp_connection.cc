#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight();
 }

size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes();
 }

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_recieving; 
 }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    if(!_active){
        return ;
    }
    _time_since_last_recieving = 0;
    // not connected, not receiving SYN and don't have ackno
    if(!_receiver.ackno().has_value()){
        if(seg.header().syn){
            _receiver.segment_received(seg);
            _sender.fill_window(); 
            connect();
        }
        return ;
    }
    else{
        
        
    }
    if (seg.header().rst){
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _rst_received = true;
        return ;
    }
    _receiver.segment_received(seg);
    if(seg.header().ack){
        _sender.ack_received(seg.header().ackno,seg.header().win);
    }
    if (_sender.segments_out().empty()){
        _sender.send_empty_segment();
    }
    if ((_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) && \
                  seg.header().seqno == _receiver.ackno().value() - 1)) {
        _sender.send_empty_segment();
    }
    queue<TCPSegment> send_queue = _sender.segments_out();
    TCPSegment temp_segment = send_queue.front();
    send_queue.pop();
    int window_size = _receiver.stream_out().remaining_capacity();
    temp_segment.header().win = min(window_size,UINT_LEAST16_MAX);
    _segments_out.push(temp_segment);
    while(!send_queue.empty()){
        _segments_out.push(send_queue.front());
        _segments_out.pop();
    }
    _timer = 0;
 }

bool TCPConnection::active() const {
    return _active;
 }

size_t TCPConnection::write(const string &data) {
    size_t length = _sender.stream_in().write(data);
    return length;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _time_since_last_recieving += ms_since_last_tick;
    if(_sender.consecutive_retransmissions() >= _cfg.MAX_RETX_ATTEMPTS){
        // unclean closing 
        unclean_shutdown();
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_sender_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}


void TCPConnection::send_sender_segments(){
    auto que = _sender.segments_out();
    TCPSegment temp_segment;
    while(!que.empty()){
        temp_segment = que.front();
        temp_segment.header().ackno = _receiver.ackno();
        int windowsize = _receiver.window_size();
        temp_segment.header().win = min(windowsize,UINT_LEAST16_MAX);
        _segments_out.push(temp_segment);
        que.pop();
    }
}


void TCPConnection::clean_shutdown(){
    if(_sender.segments_out().empty() && _receiver.stream_out().eof()){
        
    }
}


void TCPConnection::unclean_shutdown(){
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}