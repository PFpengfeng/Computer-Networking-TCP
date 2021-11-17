#include "stream_reassembler.hh"
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;


StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

// merge two Package and return the merge length.
int StreamReassembler::merge_package(Package &lift,const Package &right){
    Package x,y;
    if (lift.index <= right.index){
        x = lift;
        y = right;
    }
    else{
        x = right;
        y = lift;
    }
    // without intersection, return -1
    if (x.index + x.length < y.index){
        return -1;
    }
    // y is in x
    if (x.index + x.length > y.index + y.length){
        lift = x;
        return y.length;
    }
    size_t loc = x.index + x.length - y.index;
    x.data = x.data + y.data.substr(loc);
    x.length = x.data.size();
    lift = x;
    return loc;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index + data.size() <= _low_index){
        if (eof){
            _eof = true;
            if(_unassembled_bytes_num == 0){
                _output.end_input();
            }
        }
        return ;
    }
    Package node;
    if (index < _low_index){
        node.data = data.substr(_low_index - index);
        node.index = _low_index;
        node.length = data.size() - (_low_index - index);
    }
    else{
        node.data = data;
        node.index = index;
        node.length = data.size();
    }
    _unassembled_bytes_num += node.length;
    
    int merge_len = -1;
    // set<Package>::iterator itr = _data_buffer.begin();
    for (auto itr = _data_buffer.begin(); itr != _data_buffer.end(); itr++){
        merge_len = merge_package(node,*itr);
        if(merge_len < 0){ 
            continue;
        }
        else{
            _unassembled_bytes_num -= merge_len;
            _data_buffer.erase(itr);
        }
    }
    _data_buffer.insert(node);

    // write byte into stream;
    set<Package>::iterator itr = _data_buffer.begin();
    size_t write_len = 0;
    if (itr->index == _low_index){
        write_len += _output.write(itr->data);
        _low_index += write_len;
        _unassembled_bytes_num -= write_len;
        _data_buffer.erase(itr);
    }

    if (eof){
            _eof = true;
        }
    if (_eof && _unassembled_bytes_num ==0){
        _output.end_input();
    }
    return ;
}

size_t StreamReassembler::unassembled_bytes() const { 
	return _unassembled_bytes_num;
}

bool StreamReassembler::empty() const { return _unassembled_bytes_num==0; }
