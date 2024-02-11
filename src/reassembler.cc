#include "reassembler.hh"

#include <iterator>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  const auto try_close = [&]() -> void {
    if ( end_check_ and bytes_pending() == 0 ) {
      output_.writer().close();
    }
  };

  if ( data.empty() ) { // No capacity limit
    end_check_ |= is_last_substring;
    return try_close();
  } // special judement

  const auto L = writer().bytes_pushed(); // Reassembler's internal storage: [L, R)
  const auto R = L + writer().available_capacity();
  if ( L >= R ) {
    return; // available_capacity == 0
  }
  if ( first_index + data.size() <= L or first_index >= R ) {
    return; // Out of ranger
  }
  if ( first_index + data.size() > R ) { // Remove unacceptable bytes
    data = data.substr( 0, R - first_index );
    is_last_substring = false;
  }
  if ( first_index < L ) { // Remove poped/buffered bytes
    data = data.substr( L - first_index );
    first_index = L;
  }
  end_check_ |= is_last_substring;

  const auto split = [&]( const uint64_t& pos ) {
    auto it = buf_.lower_bound( pos );
    if ( it != buf_.end() and it->first == pos )
      return it;
    if ( it == buf_.begin() )
      return it;
    const auto pit = prev( it );
    if ( pit->first + pit->second.size() <= pos )
      return it;
    const auto str = pit->second;
    pit->second = str.substr( 0, pos - pit->first );
    return buf_.emplace( pos, str.substr( pos - pit->first ) ).first;
  };

  if ( not buf_.empty() ) {
    auto it = split( first_index );
    const auto& pr = split( first_index + data.size() );
    while ( it != pr ) {
      total_pending_ -= it->second.size();
      it = buf_.erase( it );
    }
  }
  buf_.emplace( first_index, data );
  total_pending_ += data.size();

  while ( not buf_.empty() and buf_.begin()->first == writer().bytes_pushed() ) {
    output_.writer().push( buf_.begin()->second );
    total_pending_ -= buf_.begin()->second.size();
    buf_.erase( buf_.begin() );
  }
  return try_close();
}

uint64_t Reassembler::bytes_pending() const
{
  return total_pending_;
}
