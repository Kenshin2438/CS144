#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  if ( data.size() > available_capacity() ) {
    data = data.substr( 0, available_capacity() );
  }

  buf_ += data;
  total_pushed_ += data.size();
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buf_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return total_pushed_;
}

bool Reader::is_finished() const
{
  return closed_ and buf_.empty();
}

uint64_t Reader::bytes_popped() const
{
  return total_popped_;
}

string_view Reader::peek() const
{
  return buf_;
}

void Reader::pop( uint64_t len )
{
  if ( len > bytes_buffered() ) {
    set_error();
    return;
  }

  buf_ = buf_.substr( len );
  total_popped_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  return buf_.size();
}
