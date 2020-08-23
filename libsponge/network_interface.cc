#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <arpa/inet.h>
#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
    _timer.start();
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
// This method is called when the caller (e.g., your TCPConnection or a router) wants to
// send an outbound Internet (IP) datagram to the next hop. It’s your interface’s job to
// translate this datagram into an Ethernet frame and (eventually) send it.
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // looks up the MAC of next_hop_ip
    // auto mac_it = _arp_table.find(next_hop_ip);
    optional<EthernetAddress> eth = get_arp(next_hop_ip);
    if (!eth) {
        auto it = _arp_query_time.find(next_hop_ip);
        bool not_found = it == _arp_query_time.end();
        bool timeout = _timer.time - it->second >= ARP_INTERVAL;
        if (!not_found && timeout)
            _arp_query_time.erase(it);
        if (not_found || timeout) {
            // not found and not queried in last 5 seconds, broadcast ARP
            _arp_query_time.insert_or_assign(next_hop_ip, _timer.time);
            ARPMessage arp_msg;
            arp_msg.opcode = arp_msg.OPCODE_REQUEST;
            arp_msg.sender_ethernet_address = this->_ethernet_address;
            arp_msg.sender_ip_address = this->_ip_address.ipv4_numeric();
            arp_msg.target_ip_address = next_hop_ip;
            EthernetFrame ef;
            EthernetHeader eh;
            eh.src = _ethernet_address;
            eh.dst = ETHERNET_BROADCAST;
            eh.type = eh.TYPE_ARP;
            ef.header() = eh;
            ef.payload() = arp_msg.serialize();
            // send ARP and buffer ip datagram
            this->_frames_out.push(ef);
        }
        this->_waiting_datagram.push({dgram, next_hop_ip});
    } else {
        EthernetAddress dst = eth.value();
        EthernetFrame ef;
        EthernetHeader eh;
        eh.src = _ethernet_address;
        eh.dst = dst;
        eh.type = eh.TYPE_IPv4;
        ef.header() = eh;
        ef.payload() = dgram.serialize();
        this->_frames_out.push(ef);
    }
}

std::optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // cout << "<- " << summary(frame);
    // ignore frames not destined for the network interface
    const EthernetAddress &dst = frame.header().dst;
    if (dst != ETHERNET_BROADCAST && dst != this->_ethernet_address) {
        return {};
    }
    const auto type = frame.header().type;
    if (type == EthernetHeader::TYPE_IPv4) {
        // parse the payload as InternetDatagram
        InternetDatagram dgram;
        const ParseResult &res = dgram.parse(Buffer(frame.payload()));
        if (res == ParseResult::NoError) {
            return dgram;
        }
    } else if (type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        const ParseResult &res = arp.parse(Buffer(frame.payload()));
        if (res == ParseResult::NoError) {
            uint32_t ip = arp.sender_ip_address;
            EthernetAddress eth = arp.sender_ethernet_address;
            insert_arp(ip, eth);
            if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric()) {
                // send arp reply
                ARPMessage arp_msg;
                arp_msg.opcode = arp_msg.OPCODE_REPLY;
                arp_msg.sender_ethernet_address = this->_ethernet_address;
                arp_msg.sender_ip_address = this->_ip_address.ipv4_numeric();
                arp_msg.target_ip_address = ip;
                arp_msg.target_ethernet_address = eth;
                EthernetFrame ef;
                EthernetHeader eh;
                eh.src = _ethernet_address;
                eh.dst = eth;
                eh.type = eh.TYPE_ARP;
                ef.header() = eh;
                ef.payload() = arp_msg.serialize();
                // send ARP and buffer ip datagram
                this->_frames_out.push(ef);
            }
            send_frames();
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { _timer.update(ms_since_last_tick); }

// Get EthernetAddress from IPv4 address with expiration checking
optional<EthernetAddress> NetworkInterface::get_arp(uint32_t ip_addr) {
    auto it = _arp_table.find(ip_addr);
    if (it == _arp_table.end()) {
        return std::nullopt;
    } else {
        struct arp_entry e = it->second;
        if (_timer.time - e.time >= ARP_EXPIRE) {
            _arp_table.erase(it);
            return std::nullopt;
        } else {
            return e.eth;
        }
    }
}

void NetworkInterface::insert_arp(uint32_t ip_addr, EthernetAddress eth_addr) {
    struct arp_entry arp;
    arp.eth = eth_addr;
    arp.time = _timer.time;
    _arp_table.insert_or_assign(ip_addr, arp);
    // cout << "APR: + " << inet_ntoa({htobe32(ip_addr)}) << " -> " << to_string(arp.eth) << endl;
}

void NetworkInterface::send_frames() {
    while (!_waiting_datagram.empty()) {
        pending_dgram &dgram_pair = _waiting_datagram.front();
        uint32_t dst_ip = dgram_pair.next_hop_ip;
        auto eth = get_arp(dst_ip);
        if (!eth)
            break;
        EthernetAddress dst = eth.value();
        EthernetFrame ef;
        EthernetHeader eh;
        eh.src = _ethernet_address;
        eh.dst = dst;
        eh.type = eh.TYPE_IPv4;
        ef.header() = eh;
        ef.payload() = dgram_pair.dgram.serialize();
        this->_frames_out.push(ef);
        // cout << to_string(dgram);
        _waiting_datagram.pop();
    }
}
