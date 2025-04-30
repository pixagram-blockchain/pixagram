#pragma once

#include <fc/io/raw.hpp>
#include <hive/protocol/types_fwd.hpp>
#include <hive/protocol/misc_utilities.hpp>

#define PIXA_ASSET_SYMBOL_PRECISION_BITS     4
#define PIXA_ASSET_CONTROL_BITS              1
#define PIXA_NAI_SHIFT                       ( PIXA_ASSET_SYMBOL_PRECISION_BITS + PIXA_ASSET_CONTROL_BITS )
#define SMT_MAX_NAI                          99999999
#define SMT_MIN_NAI                          1
#define SMT_MIN_NON_RESERVED_NAI             10000000
#define PIXA_ASSET_SYMBOL_NAI_LENGTH         10
#define PIXA_ASSET_SYMBOL_NAI_STRING_LENGTH  ( PIXA_ASSET_SYMBOL_NAI_LENGTH + 2 )
#define SMT_MAX_NAI_POOL_COUNT               10
#define SMT_MAX_NAI_GENERATION_TRIES         100

#define PIXA_PRECISION_PXS    (3)
#define PIXA_PRECISION_PXC   (3)
#define PIXA_PRECISION_PXP  (6)

// One's place is used for check digit, which means NAI 0-9 all have NAI data of 0 which is invalid
// This space is safe to use because it would always result in failure to convert from NAI
#define PIXA_NAI_PXS    (1)
#define PIXA_NAI_PXC   (2)
#define PIXA_NAI_VESTS  (3)

#define PIXA_ASSET_NUM_PXS    (uint32_t(((SMT_MAX_NAI + PIXA_NAI_PXS)   << PIXA_NAI_SHIFT) | PIXA_PRECISION_PXS))
#define PIXA_ASSET_NUM_PXC   (uint32_t(((SMT_MAX_NAI + PIXA_NAI_PXC)  << PIXA_NAI_SHIFT) | PIXA_PRECISION_PXC))
#define PIXA_ASSET_NUM_VESTS  (uint32_t(((SMT_MAX_NAI + PIXA_NAI_VESTS) << PIXA_NAI_SHIFT) | PIXA_PRECISION_PXP))

#ifdef IS_TEST_NET

#define VESTS_SYMBOL_U64    (uint64_t('P') | (uint64_t('X') << 8) | (uint64_t('P') << 16) | (uint64_t('T') << 24))
#define PXC_SYMBOL_U64      (uint64_t('P') | (uint64_t('X') << 8) | (uint64_t('C') << 16) | (uint64_t('T') << 24))
#define OBSOLETE_SYMBOL_U64 PXC_SYMBOL_U64
#define PXS_SYMBOL_U64      (uint64_t('P') | (uint64_t('X') << 8) | (uint64_t('S') << 16) | (uint64_t('T') << 24))
#else

#define VESTS_SYMBOL_U64    (uint64_t('P') | (uint64_t('X') << 8) | (uint64_t('P') << 16))
#define PXC_SYMBOL_U64      (uint64_t('P') | (uint64_t('X') << 8) | (uint64_t('C') << 16))
#define OBSOLETE_SYMBOL_U64 PXC_SYMBOL_U64
#define PXS_SYMBOL_U64      (uint64_t('P') | (uint64_t('X') << 8) | (uint64_t('S') << 16))

#endif

#define VESTS_SYMBOL_SER    (uint64_t(6) | (VESTS_SYMBOL_U64 << 8)) ///< VESTS|VESTS with 6 digits of precision
#define OBSOLETE_SYMBOL_SER (uint64_t(3) | (PXC_SYMBOL_U64 << 8)) ///< STEEM|TESTS with 3 digits of precision
#define OBD_SYMBOL_SER      (uint64_t(3) | (PXS_SYMBOL_U64 << 8)) ///< SBD|TBD with 3 digits of precision

#define PIXA_ASSET_MAX_DECIMALS  12

#define SMT_ASSET_NUM_PRECISION_MASK   0xF
#define SMT_ASSET_NUM_CONTROL_MASK     0x10
#define SMT_ASSET_NUM_VESTING_MASK     0x20

#define ASSET_SYMBOL_NAI_KEY      "nai"
#define ASSET_SYMBOL_DECIMALS_KEY "decimals"

namespace hive { namespace protocol {

class asset_symbol_type
{
  public:
    enum asset_symbol_space
    {
      legacy_space = 1,
      smt_nai_space = 2
    };

    explicit operator uint32_t() { return to_nai(); }

    static asset_symbol_type from_string( const std::string& str );
    // buf must have space for PIXA_ASSET_SYMBOL_NAI_STRING_LENGTH
    static asset_symbol_type from_nai_string( const char* buf, uint8_t decimal_places );
    static asset_symbol_type from_asset_num( uint32_t asset_num )
    {   asset_symbol_type result;   result.asset_num = asset_num;   return result;   }
    static uint32_t asset_num_from_nai( uint32_t nai, uint8_t decimal_places );
    static asset_symbol_type from_nai( uint32_t nai, uint8_t decimal_places )
    {   return from_asset_num( asset_num_from_nai( nai, decimal_places ) );          }
    static uint8_t damm_checksum_8digit(uint32_t value);

    std::string to_string()const;

    void to_nai_string( char* buf )const;
    std::string to_nai_string()const
    {
      char buf[ PIXA_ASSET_SYMBOL_NAI_STRING_LENGTH ];
      to_nai_string( buf );
      return std::string( buf );
    }

    uint32_t to_nai()const;

    /**Returns true when symbol represents vesting variant of the token,
      * false for liquid one.
      */
    bool is_vesting() const;
    /**Returns vesting symbol when called from liquid one
      * and liquid symbol when called from vesting one.
      * Returns back the HBD symbol if represents HBD.
      */
    asset_symbol_type get_paired_symbol() const;
    /**Returns asset_num stripped of precision holding bits.
      * \warning checking that it's SMT symbol is caller responsibility.
      */
    uint32_t get_stripped_precision_smt_num() const
    {
      return asset_num & ~( SMT_ASSET_NUM_PRECISION_MASK );
    }

    asset_symbol_space space()const;
    uint8_t decimals()const
    {  return uint8_t( asset_num & SMT_ASSET_NUM_PRECISION_MASK );    }
    void validate()const;

    friend bool operator == ( const asset_symbol_type& a, const asset_symbol_type& b )
    {  return (a.asset_num == b.asset_num);   }
    friend bool operator != ( const asset_symbol_type& a, const asset_symbol_type& b )
    {  return (a.asset_num != b.asset_num);   }
    friend bool operator <  ( const asset_symbol_type& a, const asset_symbol_type& b )
    {  return (a.asset_num <  b.asset_num);   }
    friend bool operator >  ( const asset_symbol_type& a, const asset_symbol_type& b )
    {  return (a.asset_num >  b.asset_num);   }
    friend bool operator <= ( const asset_symbol_type& a, const asset_symbol_type& b )
    {  return (a.asset_num <= b.asset_num);   }
    friend bool operator >= ( const asset_symbol_type& a, const asset_symbol_type& b )
    {  return (a.asset_num >= b.asset_num);   }

    uint32_t asset_num = 0;
};

} } // hive::protocol

#define VESTS_SYMBOL  (hive::protocol::asset_symbol_type::from_asset_num( PIXA_ASSET_NUM_VESTS ) )
#define PXC_SYMBOL   (hive::protocol::asset_symbol_type::from_asset_num( PIXA_ASSET_NUM_PXC ) )
#define PXS_SYMBOL    (hive::protocol::asset_symbol_type::from_asset_num( PIXA_ASSET_NUM_PXS ) )

FC_REFLECT(hive::protocol::asset_symbol_type, (asset_num))

namespace fc { namespace raw {

// Legacy serialization of assets
// 0000pppp aaaaaaaa bbbbbbbb cccccccc dddddddd eeeeeeee ffffffff 00000000
// Symbol = abcdef
//
// NAI serialization of assets
// aaa1pppp bbbbbbbb cccccccc dddddddd
// NAI = (MSB to LSB) dddddddd cccccccc bbbbbbbb aaa
//
// NAI internal storage of legacy assets

template< typename Stream >
inline void pack( Stream& s, const hive::protocol::asset_symbol_type& sym )
{
  switch( sym.space() )
  {
    case hive::protocol::asset_symbol_type::legacy_space:
      if( hive::protocol::serialization_mode_controller::get_current_pack() == hive::protocol::pack_type::legacy )
      {
        uint64_t ser = 0;
        switch( sym.asset_num )
        {
          case PIXA_ASSET_NUM_PXC:
            ser = OBSOLETE_SYMBOL_SER;
            break;
          case PIXA_ASSET_NUM_PXS:
            ser = OBD_SYMBOL_SER;
            break;
          case PIXA_ASSET_NUM_VESTS:
            ser = VESTS_SYMBOL_SER;
            break;
          default:
            FC_ASSERT( false, "Cannot serialize unknown asset symbol" );
        }
        pack( s, ser );
        break;
      }
      //else
      //  continue to next case
    case hive::protocol::asset_symbol_type::smt_nai_space:
      pack( s, sym.asset_num );
      break;
    default:
      FC_ASSERT( false, "Cannot serialize unknown asset symbol" );
  }
}

template< typename Stream >
inline void unpack( Stream& s, hive::protocol::asset_symbol_type& sym, uint32_t, bool )
{
  uint64_t ser = 0;
  s.read( (char*) &ser, 4 );

  switch( ser )
  {
    case OBSOLETE_SYMBOL_SER & 0xFFFFFFFF:
      s.read( ((char*) &ser)+4, 4 );
      FC_ASSERT( ser == OBSOLETE_SYMBOL_SER, "invalid asset bits" );
      sym.asset_num = PIXA_ASSET_NUM_PXC;
      break;
    case OBD_SYMBOL_SER & 0xFFFFFFFF:
      s.read( ((char*) &ser)+4, 4 );
      FC_ASSERT( ser == OBD_SYMBOL_SER, "invalid asset bits" );
      sym.asset_num = PIXA_ASSET_NUM_PXS;
      break;
    case VESTS_SYMBOL_SER & 0xFFFFFFFF:
      s.read( ((char*) &ser)+4, 4 );
      FC_ASSERT( ser == VESTS_SYMBOL_SER, "invalid asset bits" );
      sym.asset_num = PIXA_ASSET_NUM_VESTS;
      break;
    default:
      sym.asset_num = uint32_t( ser );
  }
  sym.validate();
}

} // fc::raw

inline void to_variant( const hive::protocol::asset_symbol_type& sym, fc::variant& var )
{
  try
  {
    mutable_variant_object o;
      o( ASSET_SYMBOL_NAI_KEY, sym.to_nai_string() )
        ( ASSET_SYMBOL_DECIMALS_KEY, sym.decimals() );
    var = std::move( o );
  } FC_CAPTURE_AND_RETHROW()
}

inline void from_variant( const fc::variant& var, hive::protocol::asset_symbol_type& sym )
{
  using hive::protocol::asset_symbol_type;

  try
  {
    FC_ASSERT( var.is_object(), "Asset symbol is expected to be an object." );

    auto& o = var.get_object();

    auto nai = o.find( ASSET_SYMBOL_NAI_KEY );
    FC_ASSERT( nai != o.end(), "Expected key '${key}'.", ("key", ASSET_SYMBOL_NAI_KEY) );
    FC_ASSERT( nai->value().is_string(), "Expected a string type for value '${key}'.", ("key", ASSET_SYMBOL_NAI_KEY) );

    auto decimals = o.find( ASSET_SYMBOL_DECIMALS_KEY );
    FC_ASSERT( decimals != o.end(), "Expected key '${key}'.", ("key", ASSET_SYMBOL_DECIMALS_KEY) );
    FC_ASSERT( decimals->value().is_uint64(), "Expected an unsigned integer type for value '${key}'.", ("key", ASSET_SYMBOL_DECIMALS_KEY) );
    FC_ASSERT( decimals->value().as_uint64() <= PIXA_ASSET_MAX_DECIMALS,
      "Expected decimals to be less than or equal to ${num}", ("num", PIXA_ASSET_MAX_DECIMALS) );

    sym = asset_symbol_type::from_nai_string( nai->value().as_string().c_str(), decimals->value().as< uint8_t >() );
  } FC_CAPTURE_AND_RETHROW()
}

} // fc
