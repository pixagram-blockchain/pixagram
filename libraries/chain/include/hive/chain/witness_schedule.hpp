#pragma once

#include <hive/protocol/config.hpp>

#include <algorithm>
#include <cstdint>

namespace hive { namespace chain {

class database;

void update_witness_schedule( database& db );
void reset_virtual_schedule_time( database& db );

/**
 * How many of the scheduled witnesses must agree before a hardfork vote - or a running
 * version - counts as a majority.
 *
 * Upstream fixes this at HIVE_HARDFORK_REQUIRED_WITNESSES (17), which quietly assumes the
 * schedule is always full. A chain running fewer than 17 witnesses can therefore never reach
 * the threshold and can never activate a hardfork by vote at all. Scale it by the witnesses
 * actually scheduled instead, keeping upstream's ratio of 17/21 (>= 81%) and rounding up: a
 * full schedule of 21 still needs 17, the 8 witnesses running today need 7.
 *
 * The ratio is deliberately taken from the compile-time constant. HF29 writes the scaled
 * result into witness_schedule_object::hardfork_required_witnesses so it can be read over the
 * API, and feeding that field back in here would ratchet the requirement down a little further
 * on every schedule update.
 */
inline uint32_t pixa_hardfork_quorum( uint32_t num_scheduled_witnesses )
{
  if( num_scheduled_witnesses == 0 )
    return HIVE_HARDFORK_REQUIRED_WITNESSES;

  const uint32_t scaled = ( num_scheduled_witnesses * uint32_t( HIVE_HARDFORK_REQUIRED_WITNESSES )
                            + uint32_t( HIVE_MAX_WITNESSES ) - 1 ) / uint32_t( HIVE_MAX_WITNESSES );

  return std::min( std::max( scaled, uint32_t( 1 ) ), num_scheduled_witnesses );
}

} }
