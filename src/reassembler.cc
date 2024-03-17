#include "reassembler.hh"

#include <algorithm>
#include <ranges>

using namespace std;

auto Reassembler::split( uint64_t pos ) noexcept
{
  auto it { buf_.lower_bound( pos ) };
  if ( it != buf_.end() and it->first == pos ) {
    return it;
  }
  if ( it == buf_.begin() ) { // if buf_.empty() then begin() == end()
    return it;
  }
  if ( const auto pit { prev( it ) }; pit->first + size( pit->second ) > pos ) {
    const auto res = buf_.emplace_hint( it, pos, pit->second.substr( pos - pit->first ) );
    pit->second.resize( pos - pit->first );
    return res;
  }
  return it;
};

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  const auto try_close = [&]() noexcept -> void {
    if ( end_index_.has_value() and end_index_.value() == writer().bytes_pushed() ) {
      output_.writer().close();
    }
  };

  if ( data.empty() ) { // No capacity limit
    if ( not end_index_.has_value() and is_last_substring ) {
      end_index_.emplace( first_index );
    }
    return try_close();
  }

  if ( writer().is_closed() or writer().available_capacity() == 0U ) {
    return;
  }

  // Reassembler's internal storage: [unassembled_index, unacceptable_index)
  const uint64_t unassembled_index { writer().bytes_pushed() };
  const uint64_t unacceptable_index { unassembled_index + writer().available_capacity() };
  if ( first_index + size( data ) <= unassembled_index or first_index >= unacceptable_index ) {
    return; // Out of ranger
  }
  if ( first_index + size( data ) > unacceptable_index ) { // Remove unacceptable bytes
    data.resize( unacceptable_index - first_index );
    is_last_substring = false;
  }
  if ( first_index < unassembled_index ) { // Remove poped/buffered bytes
    data.erase( 0, unassembled_index - first_index );
    first_index = unassembled_index;
  }

  if ( not end_index_.has_value() and is_last_substring ) {
    end_index_.emplace( first_index + size( data ) );
  }

  // Can be optimizated !!!
  const auto upper { split( first_index + size( data ) ) };
  const auto lower { split( first_index ) };
  ranges::for_each( ranges::subrange( lower, upper ) | views::values,
                    [&]( const auto& str ) { total_pending_ -= str.size(); } );
  total_pending_ += size( data );
  buf_.emplace_hint( buf_.erase( lower, upper ), first_index, move( data ) );

  while ( not buf_.empty() ) {
    auto&& [index, payload] { *buf_.begin() };
    if ( index != writer().bytes_pushed() ) {
      break;
    }

    total_pending_ -= size( payload );
    output_.writer().push( move( payload ) );
    buf_.erase( buf_.begin() );
  }
  return try_close();
}

uint64_t Reassembler::bytes_pending() const
{
  return total_pending_;
}
