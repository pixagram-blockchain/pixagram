#include <hive/chain/util/owner_update_limit_mgr.hpp>

#include <hive/protocol/config.hpp>

namespace hive { namespace chain { namespace util {

const char* owner_update_limit_mgr::message_hf26  = "Owner authority can only be updated twice an hour.";

bool owner_update_limit_mgr::check( const time_point_sec& head_block_time, const time_point_sec& last_time )
{
  return head_block_time - last_time > HIVE_OWNER_UPDATE_LIMIT;
}

bool owner_update_limit_mgr::check(const time_point_sec& head_block_time, const time_point_sec& previous_time, const time_point_sec& last_time )
{
  if( check( head_block_time, last_time ) )
    return true;
  else
    return check( head_block_time, previous_time );
}

const char* owner_update_limit_mgr::msg()
{
  return message_hf26;
}

} } }
