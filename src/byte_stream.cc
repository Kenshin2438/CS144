#include "byte_stream.hh"

#include <algorithm>
#include <cstdint>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  if ( Writer::is_closed() or Writer::available_capacity() == 0 or data.empty() ) {
    return;
  }

  if ( data.size() > Writer::available_capacity() ) {
    data.resize( Writer::available_capacity() );
  }

  stream_.emplace_back( move( data ) );
  total_pushed_ += stream_.back().size();

  stream_view_.emplace_back( stream_.back() );
  total_buffered_ += stream_view_.back().size();
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - total_buffered_;
}

uint64_t Writer::bytes_pushed() const
{
  return total_pushed_;
}

bool Reader::is_finished() const
{
  return closed_ and total_buffered_ == 0;
}

uint64_t Reader::bytes_popped() const
{
  return total_popped_;
}

string_view Reader::peek() const
{
  return stream_view_.empty() ? string_view {} : stream_view_.front();
}

void Reader::pop( uint64_t len )
{
  while ( not stream_.empty() and len != 0 ) {
    const uint64_t size = stream_view_.front().size();
    if ( size > len ) {
      stream_view_.front().remove_prefix( len );
      total_buffered_ -= len;
      total_popped_ += len;
      return; // with len = 0
    } else {
      stream_view_.pop_front();
      total_buffered_ -= size;
      stream_.pop_front();
      total_popped_ += size;
      len -= size;
    }
  }
}

uint64_t Reader::bytes_buffered() const
{
  return total_buffered_;
}
