#include "tcp_receiver.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <utility>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( writer().has_error() ) {
    return;
  }
  if ( message.RST ) {
    reader().set_error();
    return;
  }

  if ( not zero_point_.has_value() ) {
    if ( not message.SYN ) {
      return;
    }
    zero_point_.emplace( message.seqno );
  }

  const uint64_t checkpoint { writer().bytes_pushed() + 1 /* SYN */ }; // abs_seqno for expecting payload
  const uint64_t absolute_seqno { message.seqno.unwrap( zero_point_.value(), checkpoint ) };
  const uint64_t stream_index { absolute_seqno + static_cast<uint64_t>( message.SYN ) - 1 /* SYN */ };
  reassembler_.insert( stream_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  const uint16_t window_size { writer().available_capacity() > UINT16_MAX
                                 ? static_cast<uint16_t>( UINT16_MAX )
                                 : static_cast<uint16_t>( writer().available_capacity() ) };
  if ( zero_point_.has_value() ) {
    const uint64_t ack_for_seqno { writer().bytes_pushed() + 1 + static_cast<uint64_t>( writer().is_closed() ) };
    return { Wrap32::wrap( ack_for_seqno, zero_point_.value() ), window_size, writer().has_error() };
  }
  return { nullopt, window_size, writer().has_error() };
}
