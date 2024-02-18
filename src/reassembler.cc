#include "reassembler.hh"

#include <cstdint>
#include <iterator>
#include <utility>

using namespace std;

auto Reassembler::split( uint64_t pos ) -> mIterator
{
  auto it = buf_.lower_bound( pos );
  if ( it != buf_.end() and it->first == pos ) {
    return it;
  }
  if ( it == buf_.begin() ) { // if buf_.empty() then begin() == end()
    return it;
  }
  const auto pit = prev( it );
  if ( pit->first + pit->second.size() <= pos ) {
    return it;
  }
  auto str = pit->second;
  pit->second.resize( pos - pit->first );
  str.erase( 0, pos - pit->first );
  return buf_.emplace( pos, move( str ) ).first;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( data.empty() ) { // No capacity limit
    if ( is_last_substring and ( not end_index_.has_value() ) ) {
      end_index_.emplace( first_index );
    }
    if ( end_index_.has_value() and end_index_.value() == writer().bytes_pushed() ) {
      output_.writer().close();
    }
    return;
  }

  // Reassembler's internal storage: [unassembled_index, unacceptable_index)
  const auto unassembled_index = writer().bytes_pushed();
  const auto unacceptable_index = unassembled_index + writer().available_capacity();
  if ( unassembled_index >= unacceptable_index ) {
    return; // available_capacity == 0
  }
  if ( first_index + data.size() <= unassembled_index or first_index >= unacceptable_index ) {
    return; // Out of ranger
  }
  if ( first_index + data.size() > unacceptable_index ) { // Remove unacceptable bytes
    data.resize( unacceptable_index - first_index );
    is_last_substring = false;
  }
  if ( first_index < unassembled_index ) { // Remove poped/buffered bytes
    data.erase( 0, unassembled_index - first_index );
    first_index = unassembled_index;
  }

  if ( is_last_substring and ( not end_index_.has_value() ) ) {
    end_index_.emplace( first_index + data.size() );
  }

  const auto upper { split( first_index + data.size() ) };
  for ( auto it { split( first_index ) }; it != upper; it = buf_.erase( it ) ) {
    total_pending_ -= it->second.size();
  }
  total_pending_ += data.size();
  buf_.emplace( first_index, move( data ) );

  while ( ( not buf_.empty() ) and buf_.begin()->first == writer().bytes_pushed() ) {
    total_pending_ -= buf_.begin()->second.size();
    output_.writer().push( move( buf_.begin()->second ) );
    buf_.erase( buf_.begin() );
  }

  if ( end_index_.has_value() and end_index_.value() == writer().bytes_pushed() ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return total_pending_;
}
