#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    for (auto iter = _route_table.cbegin(); iter != _route_table.cend(); iter++) {
        if(iter->route_prefix == route_prefix && iter->prefix_length == prefix_length){
            _route_table.erase(iter);
            break;
        }
    }
    prefix_info temp_info_seg{route_prefix,prefix_length,next_hop,interface_num};
    _route_table.push_back(temp_info_seg);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint32_t dst_ip = dgram.header().dst;
    uint8_t max_match_interface_num = 1;
    std::optional<Address> max_match_add = nullopt;
    uint8_t max_match_len = 0;
    bool match_flag = false;
    for (auto iter = _route_table.cbegin(); iter != _route_table.cend(); iter++){
        if(prefix_match(iter->route_prefix,dst_ip,iter->prefix_length)){
            if(!match_flag){
                match_flag = true;
                max_match_interface_num = iter->interface_num;
                max_match_len = iter->prefix_length;
                max_match_add = iter->next_hop;
            }
            else{
                if(iter->prefix_length > max_match_len){
                    max_match_len = iter->prefix_length;
                    max_match_interface_num = iter->interface_num;
                    max_match_add = iter->next_hop;
                }
            }
        }
    }
    if(match_flag){
        if(max_match_add.has_value()){
            if(dgram.header().ttl > 1){
                dgram.header().ttl -= 1;
                interface(max_match_interface_num).send_datagram(dgram,max_match_add.value());
            }
        }
        else{
            dgram.header().ttl -= 1;
            interface(max_match_interface_num).send_datagram(dgram,Address::from_ipv4_numeric(dgram.header().dst));
        }
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}


bool Router::prefix_match(uint32_t route_prefix, uint32_t ip, const uint8_t prefix_len){
    uint8_t move_len = 32 - prefix_len;
    route_prefix = (route_prefix >> move_len);
    ip = (ip >> move_len);
    if (ip == route_prefix){
        return true;
    }
    return false;
}