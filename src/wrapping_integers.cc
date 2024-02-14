#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  /**
   * let n = [ high_32 | low_32 ]
   * wrap(n) = low_32 + zero_point.raw_value_
   *        or low_32 + zero_point.raw_value_ - 2^{32}
   */
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{ // what is the definition for the "nearest"?
  const uint64_t n_low32 { this->raw_value_ - zero_point.raw_value_ };
  const uint64_t c_low32 { checkpoint & MASK_LOW_32 };
  if ( n_low32 > c_low32 ) {
    if ( ( checkpoint & MASK_HIGH_32 ) == 0UL )
      return n_low32; // special judgement
    // ...|__checkpoint__n__|...
    const auto d1 = n_low32 - c_low32;
    const auto d2 = ( 1UL << 32 ) - d1;
    return d1 <= d2 ? checkpoint + d1 : checkpoint - d2;
  } else {
    if ( ( checkpoint & MASK_HIGH_32 ) == MASK_HIGH_32 )
      return MASK_HIGH_32 | n_low32;
    // ...|__n__checkpoint__|...
    const auto d1 = c_low32 - n_low32;
    const auto d2 = ( 1UL << 32 ) - d1;
    return d1 <= d2 ? checkpoint - d1 : checkpoint + d2;
  }
}
