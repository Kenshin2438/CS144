#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST or writer().has_error() ) {
    if ( not writer().has_error() )
      reader().set_error();
    return;
  }

  if ( not SYN_set_ ) { // Synchronization
    if ( not message.SYN ) {
      return;
    }
    SYN_set_ = true;
    zero_point_ = message.seqno;
  }

  const uint64_t checkpoint = writer().bytes_pushed() + SYN_set_ + writer().is_closed();
  const uint64_t stream_index = message.seqno.unwrap( zero_point_, checkpoint ) - 1 + message.SYN;
  reassembler_.insert( stream_index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  const uint16_t window_size = writer().available_capacity() > UINT16_MAX
                                 ? UINT16_MAX
                                 : static_cast<uint16_t>( writer().available_capacity() );
  const optional<Wrap32> ackno
    = SYN_set_ ? Wrap32::wrap( writer().bytes_pushed() + SYN_set_ + writer().is_closed(), zero_point_ )
               : optional<Wrap32> {};
  return { ackno, window_size, writer().has_error() };
}
