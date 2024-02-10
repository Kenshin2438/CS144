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
  stream_ += data;
  bytesPushed_ += data.size();
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - stream_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytesPushed_;
}

bool Reader::is_finished() const
{
  return closed_ and stream_.empty();
}

uint64_t Reader::bytes_popped() const
{
  return bytesPopped_;
}

string_view Reader::peek() const
{
  return stream_;
}

void Reader::pop( uint64_t len )
{
  if ( len > bytes_buffered() ) {
    set_error();
    return;
  }

  stream_ = stream_.substr( len );
  bytesPopped_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  return stream_.size();
}
