#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <optional>
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
  const uint16_t window_size = min( writer().available_capacity(), uint64_t { UINT16_MAX } );
  if ( not zero_point_.has_value() ) {
    return { {}, window_size, writer().has_error() };
  }
  const uint64_t absolute_seqno = writer().bytes_pushed() + 1 + static_cast<uint64_t>( writer().is_closed() );
  return { Wrap32::wrap( absolute_seqno, zero_point_.value() ), window_size, writer().has_error() };
}
