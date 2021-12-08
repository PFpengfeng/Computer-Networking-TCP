#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.payload() = move(dgram.serialize());
    if (_ip_mac_table.count(next_hop_ip) && _timer <= _ip_mac_table[next_hop_ip].ttl){
        frame.header().dst = _ip_mac_table[next_hop_ip].mac;
        _frames_out.push(frame);
    }
    else{
        _pending_arg.push(next_hop_ip);
        _retransmission_arp_frame();
        _frame_without_mac.push({frame,next_hop_ip});
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(!_ethernet_address_equal(frame.header().dst,ETHERNET_BROADCAST) && !_ethernet_address_equal(frame.header().dst,_ethernet_address)){
        return nullopt;
    }
    if(frame.header().type == EthernetHeader::TYPE_IPv4){
        InternetDatagram dgram;
        if(dgram.parse(frame.payload()) == ParseResult::NoError){
            return dgram;
        }
        else{
            return nullopt;
        }
    }
    if(frame.header().type == EthernetHeader::TYPE_ARP){
        ARPMessage arp_msg;
        if(arp_msg.parse(frame.payload()) == ParseResult::NoError){
            uint32_t ip = arp_msg.sender_ip_address;
            _ip_mac_table[ip].mac = arp_msg.sender_ethernet_address;
            _ip_mac_table[ip].ttl = _timer + 30 * 1000;  // active mappings last 30 seconds
        }
        if(arp_msg.opcode == arp_msg.OPCODE_REQUEST && arp_msg.target_ip_address == _ip_address.ipv4_numeric()){
            ARPMessage reply;
            reply.opcode = reply.OPCODE_REPLY;
            reply.sender_ethernet_address = _ethernet_address;
            reply.sender_ip_address = _ip_address.ipv4_numeric();
            reply.target_ip_address = arp_msg.sender_ip_address;
            reply.target_ethernet_address = arp_msg.sender_ethernet_address;

            EthernetFrame arp_frame;
            arp_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_frame.header().src = _ethernet_address;
            arp_frame.header().dst = arp_msg.sender_ethernet_address;
            arp_frame.payload() = move(reply.serialize());
            _frames_out.push(arp_frame);
        }
        while (!_pending_arg.empty()){
            uint32_t t_ip=_pending_arg.front();
            if(_ip_mac_table.count(t_ip) && _timer <= _ip_mac_table[t_ip].ttl){
                _pending_arg.pop();
                _pending_flag=false;
            }
            else {
                break;
            }
        }
        while (!_frame_without_mac.empty()) {
            waiting_frame node = _frame_without_mac.front();
            if (_ip_mac_table.count(node.ip) && _timer <= _ip_mac_table[node.ip].ttl) {
                node.frame.header().dst = _ip_mac_table[node.ip].mac;
                _frame_without_mac.pop();
                _frames_out.push(move(node.frame));
            } 
            else {
                break;
            }
        }
    }
    return nullopt;
}


//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    _timer += ms_since_last_tick;
    _retransmission_arp_frame();
 }

//! \param[in] addr1 one ethernet address
//! \param[in] addr2 anthor ethernet address
bool NetworkInterface::_ethernet_address_equal(EthernetAddress addr1,EthernetAddress addr2){
    for (int i=0; i<6; i++){
        if (addr1[i] != addr2[i]){
            return false;
        }
    }
    return true;
}


void NetworkInterface::_retransmission_arp_frame(){
    if(!_pending_arg.empty()){
        if(!_pending_flag || (_pending_flag && _pending_timer + 5000 < _timer)){
            _pending_timer = _timer;
            _pending_flag = true;

            uint32_t ip=_pending_arg.front();
            ARPMessage msg;
            msg.opcode = ARPMessage::OPCODE_REQUEST;
            msg.sender_ethernet_address = _ethernet_address;
            msg.sender_ip_address = _ip_address.ipv4_numeric();
            msg.target_ethernet_address = {0, 0, 0, 0, 0, 0};
            msg.target_ip_address = ip;

            EthernetFrame arp_frame;
            arp_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_frame.header().src = _ethernet_address;
            arp_frame.header().dst = ETHERNET_BROADCAST;
            arp_frame.payload() = move(msg.serialize());

            _frames_out.push(arp_frame);
        }
    }
}