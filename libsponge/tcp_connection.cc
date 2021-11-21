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
            if(_sender.bytes_in_flight() == 0){
                _receiver.segment_received(seg);
                connect();
            }
            else{
                _receiver.segment_received(seg);
                _sender.ack_received(seg.header().ackno,seg.header().win);
                _sender.send_empty_segment();
                send_sender_segments();
            }
         }
        return ;
    }
    else{
        if(seg.length_in_sequence_space() == 0 && seg.header().seqno == _receiver.ackno().value() - 1){
            _sender.send_empty_segment();
            send_sender_segments();
            return ;
        }
    }
    if(seg.header().rst){
        // receive rst and sender have sent the syn
        if(_sender.next_seqno_absolute() > 0){
            _sender.send_empty_segment();
            unclean_shutdown();
        }
        return ;
    }

    // syn sent but not acked 
    // if(seg.header().ack && seg.payload().size() != 0){
    //     return ;
    // }
    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno,seg.header().win);
    if(_sender.segments_out().empty() && seg.payload().size() > 0){
        _sender.send_empty_segment();
    }
    send_sender_segments();
    // SYN sent but not acked
    // clean_shutdown();
 }

bool TCPConnection::active() const {
    return _active;
 }

size_t TCPConnection::write(const string &data) {
    size_t length = _sender.stream_in().write(data);
    _sender.fill_window();
    send_sender_segments();
    return length;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _time_since_last_recieving += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS && _active){
        // unclean closing 
        unclean_shutdown();
        return;
    }
    send_sender_segments();
    clean_shutdown();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_sender_segments();
    clean_shutdown();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_sender_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            unclean_shutdown();

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}


void TCPConnection::send_sender_segments(){
    TCPSegment temp_segment;
    while(!_sender.segments_out().empty()){
        temp_segment = _sender.segments_out().front();
        if(_receiver.ackno().has_value()){
            temp_segment.header().ack = true;
            temp_segment.header().ackno = _receiver.ackno().value();
            int windowsize = _receiver.window_size();
            temp_segment.header().win = min(windowsize,UINT_LEAST16_MAX);
        }
        _segments_out.push(temp_segment);
        _sender.segments_out().pop();
    }
    clean_shutdown();
}


void TCPConnection::clean_shutdown(){
    if(_sender.segments_out().empty() && _receiver.stream_out().eof()){
        _linger_after_streams_finish = false;
        if(!_linger_after_streams_finish && _time_since_last_recieving > 10 * _cfg.rt_timeout){
            _active = false;
        }
        if(_sender.fin_sent() and _sender.bytes_in_flight() == 0){
            _active = false;
        }
    }

}


void TCPConnection::unclean_shutdown(){
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    if(_sender.segments_out().empty()){
        _sender.send_empty_segment();
    }
    TCPSegment temp_segment = _sender.segments_out().front();
    temp_segment.header().ack = true;
    temp_segment.header().ackno = _receiver.ackno().value();
    temp_segment.header().rst = true;
    _segments_out.push(temp_segment);
}