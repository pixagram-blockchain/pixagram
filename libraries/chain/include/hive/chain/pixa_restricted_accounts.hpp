#pragma once

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>

namespace hive { namespace chain {

using hive::protocol::account_name_type;
using hive::protocol::authority;
using hive::flat_set; // hive/protocol/types.hpp pulls fc::flat_set into namespace hive, not hive::protocol

/**
  * pixa.rex (sales) and pixa.team hold the genesis VESTS allocations. Both are
  * restricted to VESTS transfers only - plus account_update/account_update2 so
  * the signers can rotate keys - and every other operation is rejected.
  */
inline bool is_pixa_ico( const account_name_type& n )
{
  return n == PIXA_ICO_ACCOUNT || n == PIXA_TEAM_ACCOUNT;
}

/**
  * Defined once, in hive_evaluator.cpp. Assertion tags have to be unique across
  * the whole chain library (see assertion_id_verifier), so this cannot be an
  * inline helper - each translation unit that inlined it would emit the same
  * assertion id and the verifier build would fail on the redefinition.
  */
void pixa_only_vests_transfer_assert( const char* msg );

inline void assert_not_pixa_ico( const account_name_type& n, const char* msg )
{
  if( is_pixa_ico( n ) )
    pixa_only_vests_transfer_assert( msg );
}

inline void assert_not_pixa_ico( const flat_set< account_name_type >& names, const char* msg )
{
  for( const auto& n : names )
  {
    if( is_pixa_ico( n ) )
      pixa_only_vests_transfer_assert( msg );
  }
}

inline void assert_not_pixa_ico( const authority& auth, const char* msg )
{
  for( const auto& item : auth.account_auths )
  {
    if( is_pixa_ico( item.first ) )
      pixa_only_vests_transfer_assert( msg );
  }
}

} } // hive::chain
