#pragma once

#include <hive/protocol/asset.hpp>

#include <fc/uint128.hpp>

namespace hive { namespace chain { namespace util {

/**
 * Pixagram HF30: the PXS the treasury receives for one block's DPF share, without throwing away
 * the part below 0.001 PXS.
 *
 * Before HF30 each block converts its share with `funds * median`, which truncates to 0.001 PXS.
 * At Pixagram's scale one block's share is a few thousandths of a PXS (0.0027 at a 51.833 PIXA
 * feed), so truncation cut the fund from 15% to about 11% of issuance.
 *
 * Here block n pays floor((n+1)x) - floor(nx), where x is the exact share in units of 0.001 PXS.
 * Each block pays floor(x) or ceil(x), and consecutive blocks telescope to floor(N*x), so the
 * treasury receives the exact share on average. It needs no stored remainder and depends only on
 * the block number, so every node computes the same amount.
 */
inline protocol::HBD_asset dhf_funding_without_truncation( const protocol::HIVE_asset& funds,
  const protocol::HBD_price& median, uint32_t block_num )
{
  if( funds.get_amount() <= 0 || median.is_null() )
    return protocol::HBD_asset( 0 );

  // HBD_price is PXS (base) per PIXA (quote): PIXA -> PXS multiplies by base and divides by quote.
  const fc::uint128_t pxs_side  = median.get_base().get_amount();
  const fc::uint128_t pixa_side = median.get_quote().get_amount();
  const fc::uint128_t per_block = fc::uint128_t( funds.get_amount() ) * pxs_side; // milli-PXS * pixa_side
  const fc::uint128_t n = block_num;
  const fc::uint128_t amount = ( ( n + 1 ) * per_block ) / pixa_side - ( n * per_block ) / pixa_side;
  return protocol::HBD_asset( fc::uint128_to_uint64( amount ) );
}

} } } // hive::chain::util
