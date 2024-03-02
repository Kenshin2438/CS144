#include <iostream>
#include <iterator>
#include <utility>

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

static ARPMessage make_arp( const uint16_t& opcode,
                            const EthernetAddress& sender_ethernet_address,
                            const Address& sender_ip_address,
                            const EthernetAddress& target_ethernet_address,
                            const Address& target_ip_address )
{
  ARPMessage arp;
  arp.opcode = opcode;
  arp.sender_ethernet_address = sender_ethernet_address;
  arp.sender_ip_address = sender_ip_address.ipv4_numeric();
  arp.target_ethernet_address = target_ethernet_address;
  arp.target_ip_address = target_ip_address.ipv4_numeric();
  return arp;
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  if ( ARP_cache_.contains( next_hop ) ) {
    const EthernetAddress& dst { ARP_cache_[next_hop].first };
    return transmit( { { dst, ethernet_address_, EthernetHeader::TYPE_IPv4 }, serialize( dgram ) } );
  }

  dgrams_waitting_[next_hop].emplace_back( dgram );
  if ( not waitting_timer_.contains( next_hop ) ) {
    waitting_timer_.emplace( next_hop, NetworkInterface::Timer {} );
    const auto arp_request { make_arp( ARPMessage::OPCODE_REQUEST, ethernet_address_, ip_address_, {}, next_hop ) };
    transmit( { { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP }, serialize( arp_request ) } );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ and frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram ipv4_datagram;
    if ( parse( ipv4_datagram, frame.payload ) ) {
      datagrams_received_.emplace( move( ipv4_datagram ) );
    }
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage msg;
    if ( not parse( msg, frame.payload ) ) {
      return;
    }

    const Address& next_hop { Address::from_ipv4_numeric( msg.sender_ip_address ) };
    const EthernetAddress& dst { msg.sender_ethernet_address };
    ARP_cache_[next_hop] = { dst, NetworkInterface::Timer {} };

    if ( msg.opcode == ARPMessage::OPCODE_REQUEST and msg.target_ip_address == ip_address_.ipv4_numeric() ) {
      const auto arp_reply { make_arp( ARPMessage::OPCODE_REPLY, ethernet_address_, ip_address_, dst, next_hop ) };
      transmit( { { dst, ethernet_address_, EthernetHeader::TYPE_ARP }, serialize( arp_reply ) } );
    }

    if ( dgrams_waitting_.contains( next_hop ) ) {
      for ( const auto& dgram : dgrams_waitting_[next_hop] ) {
        transmit( { { dst, ethernet_address_, EthernetHeader::TYPE_IPv4 }, serialize( dgram ) } );
      }
      dgrams_waitting_.erase( next_hop );
      waitting_timer_.erase( next_hop );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto it = ARP_cache_.begin(); it != ARP_cache_.end(); ) {
    auto&& [_, timer] { it->second };
    if ( timer.tick( ms_since_last_tick ).expired( NetworkInterface::Timer::ARP_ENTRY_TTL_ms ) ) {
      it = ARP_cache_.erase( it );
    } else {
      it = next( it );
    }
  }

  for ( auto it = waitting_timer_.begin(); it != waitting_timer_.end(); ) {
    auto&& timer { it->second };
    if ( timer.tick( ms_since_last_tick ).expired( NetworkInterface::Timer::ARP_RESPONSE_TTL_ms ) ) {
      it = waitting_timer_.erase( it );
    } else {
      it = next( it );
    }
  }
}
