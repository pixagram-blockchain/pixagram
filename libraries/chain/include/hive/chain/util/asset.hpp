#pragma once

#include <hive/protocol/asset.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::asset;
using hive::protocol::price;

inline asset to_hbd( const price& p, const asset& hive )
{
  FC_ASSERT( hive.symbol == PXC_SYMBOL );
  if( p.is_null() )
    return asset( 0, PXS_SYMBOL );
  return hive * p;
}

inline asset to_hive( const price& p, const asset& hbd )
{
  FC_ASSERT( hbd.symbol == PXS_SYMBOL );
  if( p.is_null() )
    return asset( 0, PXC_SYMBOL );
  return hbd * p;
}

} } }
