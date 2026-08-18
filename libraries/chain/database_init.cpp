#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/irreversible_block_data.hpp>
#include <hive/chain/notifications.hpp>
#include <hive/chain/rc/rc_utility.hpp>
#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>
#include <hive/chain/hardfork_property_object_multiindex.hpp>
#include <hive/chain/block_summary_object_multiindex.hpp>
#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>

#include <hive/chain/util/rd_setup.hpp>
#include <hive/chain/util/state_checker_tools.hpp>

#include <hive/protocol/get_config.hpp>

#include <appbase/plugin.hpp>

#include "database_impl.hpp"

// TODO(real-mainnet): re-enable 3-of-3 multisig for pixa.ico / pixa.fund by
// uncommenting this guard, the helpers below, and the multisig branch in
// init_genesis(), plus the matching CMakeLists.txt block.
//#ifndef IS_TEST_NET
//#ifndef PIXA_MULTISIG_KEY1_PUBLIC_KEY_STR
//#error "Define PIXA_MULTISIG_KEY1_PUBLIC_KEY_STR for non-testnet builds"
//#endif
//#ifndef PIXA_MULTISIG_KEY2_PUBLIC_KEY_STR
//#error "Define PIXA_MULTISIG_KEY2_PUBLIC_KEY_STR for non-testnet builds"
//#endif
//#ifndef PIXA_MULTISIG_KEY3_PUBLIC_KEY_STR
//#error "Define PIXA_MULTISIG_KEY3_PUBLIC_KEY_STR for non-testnet builds"
//#endif
//#endif

namespace hive { namespace chain {

void database::initialize_evaluators()
{
  register_social_evaluators( _my->_evaluator_registry );
  register_transfer_evaluators( _my->_evaluator_registry );
  register_account_evaluators( _my->_evaluator_registry );
  register_witness_evaluators( _my->_evaluator_registry );
  register_dhf_evaluators( _my->_evaluator_registry );

  rc().initialize_evaluators();
}


void database::register_custom_operation_interpreter( std::shared_ptr< custom_operation_interpreter > interpreter )
{
  FC_ASSERT( interpreter );
  bool inserted = _custom_operation_interpreters.emplace( interpreter->get_custom_id(), interpreter ).second;
  // This assert triggering means we're mis-configured (multiple registrations of custom JSON evaluator for same ID)
  FC_ASSERT( inserted );
}

std::shared_ptr< custom_operation_interpreter > database::get_custom_json_evaluator( const custom_id_type& id )
{
  auto it = _custom_operation_interpreters.find( id );
  if( it != _custom_operation_interpreters.end() )
    return it->second;
  return std::shared_ptr< custom_operation_interpreter >();
}

void initialize_core_indexes( database& db );

void database::initialize_indexes()
{
  initialize_core_indexes( *this );
  _my->_plugin_index_signal();
}

void database::initialize_irreversible_storage()
{
  auto s = get_segment_manager();
  last_irreversible_object = s->find_or_construct<irreversible_block_data_type>( "irreversible" )(
    allocator< irreversible_block_data_type >( s )
  );

  cached_lib = last_irreversible_object->_irreversible_block_data.create_full_block();
}

void database::verify_match_of_state_objects_definitions_from_shm()
{
  FC_ASSERT(_my->_decoded_types_data_storage);
  const std::string shm_decoded_state_objects_data = get_decoded_state_objects_data_from_shm();

  if (shm_decoded_state_objects_data.empty())
    set_decoded_state_objects_data(_my->_decoded_types_data_storage->generate_decoded_types_data_json_string());
  else
    util::verify_match_of_state_definitions(*(_my->_decoded_types_data_storage), shm_decoded_state_objects_data);

  _my->delete_decoded_types_data_storage();
}

void database::verify_match_of_blockchain_configuration()
{
  fc::mutable_variant_object current_blockchain_config = protocol::get_config(get_treasury_name(), get_chain_id());
  fc::variant full_current_blockchain_config_as_variant;
  fc::to_variant(current_blockchain_config, full_current_blockchain_config_as_variant);
  const std::string full_current_blockchain_config_as_json_string = fc::json::to_string(full_current_blockchain_config_as_variant);

  const std::string full_stored_blockchain_config_json = get_blockchain_config_from_shm();

  if (full_stored_blockchain_config_json.empty())
    set_blockchain_config(full_current_blockchain_config_as_json_string);
  else if (full_stored_blockchain_config_json != full_current_blockchain_config_as_json_string)
    util::verify_match_of_blockchain_configuration(current_blockchain_config, full_current_blockchain_config_as_variant, full_stored_blockchain_config_json);
}

std::string database::get_current_decoded_types_data_json()
{
  FC_ASSERT(_my->_decoded_types_data_storage && "No storage - no types");
  const std::string decoded_types_data_json = _my->_decoded_types_data_storage->generate_decoded_types_data_json_string();
  _my->delete_decoded_types_data_storage();
  return decoded_types_data_json;
}

const std::string& database::get_json_schema()const
{
  return _json_schema;
}

void database::init_schema()
{
  /*done_adding_indexes();

  db_schema ds;

  std::vector< std::shared_ptr< abstract_schema > > schema_list;

  std::vector< object_schema > object_schemas;
  get_object_schemas( object_schemas );

  for( const object_schema& oschema : object_schemas )
  {
    ds.object_types.emplace_back();
    ds.object_types.back().space_type.first = oschema.space_id;
    ds.object_types.back().space_type.second = oschema.type_id;
    oschema.schema->get_name( ds.object_types.back().type );
    schema_list.push_back( oschema.schema );
  }

  std::shared_ptr< abstract_schema > operation_schema = get_schema_for_type< operation >();
  operation_schema->get_name( ds.operation_type );
  schema_list.push_back( operation_schema );

  for( const std::pair< std::string, std::shared_ptr< custom_operation_interpreter > >& p : _custom_operation_interpreters )
  {
    ds.custom_operation_types.emplace_back();
    ds.custom_operation_types.back().id = p.first;
    schema_list.push_back( p.second->get_operation_schema() );
    schema_list.back()->get_name( ds.custom_operation_types.back().type );
  }

  graphene::db::add_dependent_schemas( schema_list );
  std::sort( schema_list.begin(), schema_list.end(),
    []( const std::shared_ptr< abstract_schema >& a,
        const std::shared_ptr< abstract_schema >& b )
    {
      return a->id < b->id;
    } );
  auto new_end = std::unique( schema_list.begin(), schema_list.end(),
    []( const std::shared_ptr< abstract_schema >& a,
        const std::shared_ptr< abstract_schema >& b )
    {
      return a->id == b->id;
    } );
  schema_list.erase( new_end, schema_list.end() );

  for( std::shared_ptr< abstract_schema >& s : schema_list )
  {
    std::string tname;
    s->get_name( tname );
    FC_ASSERT( ds.types.find( tname ) == ds.types.end(), "types with different ID's found for name ${tname}", ("tname", tname) );
    std::string ss;
    s->get_str_schema( ss );
    ds.types.emplace( tname, ss );
  }

  _json_schema = fc::json::to_string( ds );
  return;*/
}

// TODO(real-mainnet): re-enable these helpers together with the multisig branch
// in init_genesis() to restore 3-of-3 multisig for pixa.ico / pixa.fund.
//namespace {
//
//public_key_type get_pixa_multisig_public_key( const char* test_seed, const char* configured_key )
//{
//#ifdef IS_TEST_NET
//  return fc::ecc::private_key::regenerate( fc::sha256::hash( std::string( test_seed ) ) ).get_public_key();
//#else
//  return public_key_type( configured_key );
//#endif
//}
//
//authority make_three_of_three_authority( const public_key_type& key1, const public_key_type& key2, const public_key_type& key3 )
//{
//  authority auth;
//  auth.weight_threshold = 3;
//  auth.add_authority( key1, 1 );
//  auth.add_authority( key2, 1 );
//  auth.add_authority( key3, 1 );
//  return auth;
//}
//
//} // anonymous namespace

void database::init_genesis()
{
  try
  {
    struct auth_inhibitor
    {
      auth_inhibitor(database& db) : db(db), old_flags(db.get_node_skip_flags())
      { db.set_node_skip_flags( old_flags | skip_authority_check ); }
      ~auth_inhibitor()
      { db.set_node_skip_flags( old_flags ); }
    private:
      database& db;
      uint32_t old_flags;
    } inhibitor(*this);

    // Create blockchain accounts
    public_key_type      init_public_key(HIVE_INIT_PUBLIC_KEY);
    // Pixa genesis 3-of-3 multisig authorities: each account is controlled by
    // three independent signers and all three signatures are required
    // (weight_threshold == 3). The memo key is a single key (not consensus)
    // taken from signer 1 of each account.
    const auto make_3of3 = []( const char* k1, const char* k2, const char* k3 )
    {
      authority a;
      a.weight_threshold = 3;
      a.add_authority( public_key_type( k1 ), 1 );
      a.add_authority( public_key_type( k2 ), 1 );
      a.add_authority( public_key_type( k3 ), 1 );
      return a;
    };

    const char* const shared_owner   = "PIX75hHsAAqhkmbr2pWbXn1vsvr3PRgyx6tHRJBpA5N1hp1Vthaco";
    const char* const shared_active  = "PIX8G2M2bsDbNNHutc96sXSjB6RgwMi5a9dwYwFGqSFLsYy4D42B3";
    const char* const shared_posting = "PIX8PXgrgPDFZV9xeZKhFpUk2eJCeh2qHNEtD1tgRXdJ7YYNxgrYN";

    // pixa.rex (sales) = signer A + signer C + shared 3rd signer (3-of-3)
    const authority       rex_owner    = make_3of3( "PIX59byAgVv77TwmADzsbnu8FA6BqDN5bCNFbJC7x2nQnKiNMfgTv", "PIX7u8cqq22hHzKzi3hGnPEQxeYcvUxK9zu5pbqJPzeMXxt1JJScB", shared_owner );
    const authority       rex_active   = make_3of3( "PIX83FkdNTduEweHBBp9ombdF6GZE1RTCNCno36TSv3KxtLHCMPsi", "PIX7hZGJvAmYhRkxz64n7hzVDf73sAMPgkNZSaKsCncW5m5yinyoK", shared_active );
    const authority       rex_posting  = make_3of3( "PIX7qKJvBkMoAjqnt7CUsx7CSjvcHzTwBGXByeuiG6vb4AE532NHN", "PIX7DRsi6FyfPAYrtWWydqUb4eoSKt24pyKWGnS1qhGFcL6t1MhER", shared_posting );
    const public_key_type rex_memo(  "PIX5kMDaZdiLqaE5gFDwE4ie9PGvXe1J9yDyezHhWei3qJE3GDS1j" );

    // pixa.team (team & advisors) = signer B + signer D + shared 3rd signer (3-of-3)
    const authority       team_owner   = make_3of3( "PIX5xyaAasUNGsSgAFBjbhsAfNz3ouSxuGfTRjBKJLmrogp3kCsjN", "PIX7wer24ameAxo37KftBWuzaCMeRUybcVtYAokiEu2ksLdbmLtdK", shared_owner );
    const authority       team_active  = make_3of3( "PIX6XNMz2C2smo5tzLfYziQpkz1e3apXQG4YHJBkRGm9426Kr6ZDD", "PIX7TMqA9JBDe9qYNLCvoaDyqGga1Yx8993o6U7aB2uSepGE2fnvt", shared_active );
    const authority       team_posting = make_3of3( "PIX6D4SVPEf1YFUhK3gfq3u53Sfmy3GhTf8NvfUn8xuvQXiEbRnaM", "PIX5w8bqbjwvfypCYje6p8T4NjptRNwqJg2e2WuDUW9eyZWRqjHFc", shared_posting );
    const public_key_type team_memo( "PIX7EoXL9gboyYtcrA1dPR6ydVzyUySSi9qetikKVMeQkdMzhDzzi" );

    create< account_object >( PIXA_ICO_ACCOUNT, HIVE_GENESIS_TIME, rex_memo );

    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = PIXA_ICO_ACCOUNT;
      auth.owner = rex_owner;
      auth.active = rex_active;
      auth.posting = rex_posting;
    });

    create< account_object >( HIVE_MINER_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_MINER_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    });

    create< account_object >( HIVE_NULL_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_NULL_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    });

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED)
    create< account_object >( OBSOLETE_TREASURY_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >([&](account_authority_object& auth)
    {
      auth.account = OBSOLETE_TREASURY_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );
#endif

    create< account_object >( NEW_HIVE_TREASURY_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >([&](account_authority_object& auth)
    {
      // No keys: HF21 (which fires at block 1 on a fresh chain) calls
      // lock_account() on the treasury and wipes any authority we'd set
      // here. Funds leave the treasury only via DAO proposal payments.
      auth.account = NEW_HIVE_TREASURY_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );

    create< account_object >( PIXA_TEAM_ACCOUNT, HIVE_GENESIS_TIME, team_memo );
    create< account_authority_object >([&](account_authority_object& auth)
    {
      auth.account = PIXA_TEAM_ACCOUNT;
      auth.owner = team_owner;
      auth.active = team_active;
      auth.posting = team_posting;
    } );

    create< account_object >( HIVE_TEMP_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_TEMP_ACCOUNT;
      auth.owner.weight_threshold = 0;
      auth.active.weight_threshold = 0;
      auth.posting.weight_threshold = 0;
    });

    const auto init_witness = [&]( const account_name_type& account_name )
    {
      create< account_object >( account_name, HIVE_GENESIS_TIME, init_public_key );

      create< account_authority_object >( [&]( account_authority_object& auth )
      {
        auth.account = account_name;
        auth.owner.add_authority( init_public_key, 1 );
        auth.owner.weight_threshold = 1;
        auth.active  = auth.owner;
        auth.posting = auth.active;
      });

      create< witness_object >( [&]( witness_object& w )
      {
        w.owner        = account_name;
        w.signing_key  = init_public_key;
        w.schedule = witness_object::miner;
        // Genesis-only override: account creation is free at launch so the
        // network can bootstrap. Direct chainbase write bypasses the global
        // HIVE_MIN_ACCOUNT_CREATION_FEE validation; once witnesses come
        // online and call witness_set_properties_operation any new fee they
        // publish must satisfy that minimum.
        w.props.account_creation_fee = HIVE_asset( 0 );
      } );
    };

    for( int i = 0; i < HIVE_NUM_INIT_MINERS; ++i )
      init_witness( HIVE_INIT_MINER_NAME + ( i ? fc::to_string( i ) : std::string() ) );

#ifdef USE_ALTERNATE_CHAIN_ID
    for( const auto& witness : configuration_data.get_init_witnesses() )
      init_witness( witness );
#endif

    // The "steem" account exists as a placeholder so legacy code paths that
    // call get_account("steem") (e.g. the pre-HF11 recovery_account fallback)
    // don't fail at lookup. Pixagram is not a Steem fork; the account has no
    // keys and its authority is unsatisfiable so nobody can ever sign as it.
    {
      const char* STEEM_ACCOUNT_NAME = "steem";
      create< account_object >( STEEM_ACCOUNT_NAME, public_key_type(), HIVE_GENESIS_TIME, HIVE_GENESIS_TIME, true, nullptr, true, VEST_asset( 0 ) );
      create< account_authority_object >( [&]( account_authority_object& auth )
      {
        auth.account = STEEM_ACCOUNT_NAME;
        auth.owner.weight_threshold = 1;
        auth.active.weight_threshold = 1;
        auth.posting.weight_threshold = 1;
      } );
    }

    const auto& dgpo = create< dynamic_global_property_object >( HIVE_INIT_MINER_NAME );
    const VEST_asset ico_vests( asset( 75000000000000ll, VESTS_SYMBOL ) );    // 75 M PP for pixa.rex (sales)
    const VEST_asset team_vests( asset( 25000000000000ll, VESTS_SYMBOL ) );   // 25 M PP for pixa.team
    const HBD_asset  omnibus_hbd( asset( 245098039ll, HBD_SYMBOL ) );         // ~245 098 PXS = 25 M PIXA-equivalent of 25 M VESTS (1:1 vesting price) at genesis median 1 PXS = 102 PIXA
    const HIVE_asset ico_fund = ico_vests * HIVE_INITIAL_VESTING_PRICE;
    const HIVE_asset team_fund = team_vests * HIVE_INITIAL_VESTING_PRICE;

    modify( get_account( PIXA_ICO_ACCOUNT ), [&]( account_object& a )
    {
      a.vesting_shares = ico_vests;
    } );

    modify( get_account( NEW_HIVE_TREASURY_ACCOUNT ), [&]( account_object& a )
    {
      // Treasury holds liquid PXS only - VESTS would be locked unspendable
      // by HF21's lock_account, and the proposal payout pipeline draws
      // exclusively from hbd_balance.
      a.hbd_balance = omnibus_hbd;
    } );

    modify( get_account( PIXA_TEAM_ACCOUNT ), [&]( account_object& a )
    {
      a.vesting_shares = team_vests;
    } );

    modify( dgpo, [&]( dynamic_global_property_object& gpo )
    {
      gpo.total_vesting_shares += ico_vests + team_vests;
      gpo.total_vesting_fund_hive += ico_fund + team_fund;
      gpo.current_supply += ico_fund + team_fund;
      gpo.current_hbd_supply += omnibus_hbd;
    } );

    // Seed feed history BEFORE update_virtual_supply (which reads it via
    // get_feed_history to convert PXS -> PIXA-equivalent for virtual_supply).
#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED)
    create< feed_history_object >( [&]( feed_history_object& o )
    {
      o.current_median_history = HBD_price( 1, 1 );
      o.market_median_history = o.current_median_history;
      o.current_min_history = o.current_median_history;
      o.current_max_history = o.current_median_history;
    } );
#else
    // Mainnet seed: 1 PXS = 102 PIXA so PXS-denominated allocations have a
    // PIXA-equivalent for conversions before witnesses publish their feeds.
    create< feed_history_object >( [&]( feed_history_object& o )
    {
      o.current_median_history = HBD_price( 1000, 102000 );
      o.market_median_history = o.current_median_history;
      o.current_min_history = o.current_median_history;
      o.current_max_history = o.current_median_history;
    });
#endif

    // Create hardfork_property_object BEFORE update_virtual_supply (which
    // reads it via get_hardfork_property_object).
    create< hardfork_property_object >( HIVE_GENESIS_TIME );

    // Recompute virtual_supply now that current_supply (PIXA) and
    // current_hbd_supply (PXS) have been seeded; uses the genesis median feed.
    update_virtual_supply();

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED)
    // issue initial token supply to balance of first miner
    if( HIVE_INIT_SUPPLY != 0 || HIVE_HBD_INIT_SUPPLY != 0 )
    {
      HIVE_asset to_vest( HIVE_INITIAL_VESTING );
      VEST_asset initial_vests( to_vest * HIVE_INITIAL_VESTING_PRICE );

      modify( get_account( HIVE_INIT_MINER_NAME ), [&]( account_object& a )
      {
        a.balance = HIVE_asset( HIVE_INIT_SUPPLY ) - to_vest;
        a.hbd_balance = HBD_asset( HIVE_HBD_INIT_SUPPLY );
        a.vesting_shares = initial_vests;
        FC_ASSERT( a.balance.amount >= 0 && a.hbd_balance.amount >= 0 && a.vesting_shares.amount >= 0, "Invalid testnet configuration" );
      } );
      modify( dgpo, [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += HIVE_asset( HIVE_INIT_SUPPLY );
        gpo.current_hbd_supply += HBD_asset( HIVE_HBD_INIT_SUPPLY );
        gpo.init_hbd_supply = HBD_asset( HIVE_HBD_INIT_SUPPLY );
        gpo.total_vesting_fund_hive += to_vest;
        gpo.total_vesting_shares += initial_vests;
      } );
      update_virtual_supply();
    }
#else
    FC_ASSERT( HIVE_INIT_SUPPLY == 0 && HIVE_HBD_INIT_SUPPLY == 0, "Wrong configuration" );
      // for mainnet these values must be 0, mirrornet should be compatible
#endif

    for( int i = 0; i < 0x10000; i++ )
      create< block_summary_object >( [&]( block_summary_object& ) {});

    // Create witness scheduler
    create< witness_schedule_object >( [&]( witness_schedule_object& wso )
    {
      FC_TODO( "Copied from witness_schedule.cpp, do we want to abstract this to a separate function?" );
      wso.current_shuffled_witnesses[0] = HIVE_INIT_MINER_NAME;
      // Seed median to match initminer's genesis override so account creation
      // is free from block 1, before the first witness-schedule recomputation.
      wso.median_props.account_creation_fee = HIVE_asset( 0 );
      util::rd_system_params account_subsidy_system_params;
      account_subsidy_system_params.resource_unit = HIVE_ACCOUNT_SUBSIDY_PRECISION;
      account_subsidy_system_params.decay_per_time_unit_denom_shift = HIVE_RD_DECAY_DENOM_SHIFT;
      util::rd_user_params account_subsidy_user_params;
      account_subsidy_user_params.budget_per_time_unit = wso.median_props.account_subsidy_budget;
      account_subsidy_user_params.decay_per_time_unit = wso.median_props.account_subsidy_decay;

      util::rd_user_params account_subsidy_per_witness_user_params;
      int64_t w_budget = wso.median_props.account_subsidy_budget;
      w_budget = (w_budget * HIVE_WITNESS_SUBSIDY_BUDGET_PERCENT) / HIVE_100_PERCENT;
      w_budget = std::min( w_budget, int64_t(std::numeric_limits<int32_t>::max()) );
      uint64_t w_decay = wso.median_props.account_subsidy_decay;
      w_decay = (w_decay * HIVE_WITNESS_SUBSIDY_DECAY_PERCENT) / HIVE_100_PERCENT;
      w_decay = std::min( w_decay, uint64_t(std::numeric_limits<uint32_t>::max()) );

      account_subsidy_per_witness_user_params.budget_per_time_unit = int32_t(w_budget);
      account_subsidy_per_witness_user_params.decay_per_time_unit = uint32_t(w_decay);

      util::rd_setup_dynamics_params( account_subsidy_user_params, account_subsidy_system_params, wso.account_subsidy_rd );
      util::rd_setup_dynamics_params( account_subsidy_per_witness_user_params, account_subsidy_system_params, wso.account_subsidy_witness_rd );
    } );
  }
  FC_CAPTURE_AND_RETHROW()
}

void database::set_flush_interval( uint32_t flush_blocks )
{
  _flush_blocks = flush_blocks;
  _next_flush_block = 0;
}

// Helper class for signal handler benchmarking
template <typename TFunction> struct fcall {};

template <typename TResult, typename... TArgs>
struct fcall<TResult(TArgs...)>
{
  using TNotification = std::function<TResult(TArgs...)>;

  fcall() = default;
  fcall(const TNotification& func, util::advanced_benchmark_dumper& dumper,
    const abstract_plugin& plugin, const std::string& context, const std::string& item_name)
    : _func(func), _benchmark_dumper(dumper), _context(context), _name(item_name) {}

  void operator () (TArgs&&... args)
  {
    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.begin();

    _func(std::forward<TArgs>(args)...);

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( _context, _name );
  }

private:
  TNotification                    _func;
  util::advanced_benchmark_dumper& _benchmark_dumper;
  std::string                      _context;
  std::string                      _name;
};

template <typename TResult, typename... TArgs>
struct fcall<std::function<TResult(TArgs...)>>
  : public fcall<TResult(TArgs...)>
{
  typedef fcall<TResult(TArgs...)> TBase;
  using TBase::TBase;
};

// Local helper function for connecting signal handlers with benchmarking support
template <bool IS_PRE_OPERATION, typename TSignal, typename TNotification>
database::signal_connection_ptr connect_signal_impl( TSignal& signal, const TNotification& func,
  util::advanced_benchmark_dumper& benchmark_dumper,
  const abstract_plugin& plugin, int32_t group, const std::string& item_name )
{
  fcall<TNotification> fcall_wrapper( func, benchmark_dumper, plugin,
    util::advanced_benchmark_dumper::generate_context_desc<IS_PRE_OPERATION>( plugin.get_name() ), item_name );

  return hive::utilities::make_signal_connection_ptr( signal.connect(group, fcall_wrapper) );
}

database::signal_connection_ptr database::add_pre_apply_operation_handler( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  std::string context = util::advanced_benchmark_dumper::generate_context_desc< true >( plugin.get_name() );
  auto complex_func = [this, func, context]( const operation_notification& o )
  {
    std::string name;

    if (_benchmark_dumper.is_enabled())
    {
      name = o.op.get_stored_type_name();
      _benchmark_dumper.begin();
    }

    func( o );

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( context, name );
  };

  return hive::utilities::make_signal_connection_ptr( _my->_pre_apply_operation_signal.connect(group, complex_func) );
}

database::signal_connection_ptr database::add_post_apply_operation_handler( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  std::string context = util::advanced_benchmark_dumper::generate_context_desc< false >( plugin.get_name() );
  auto complex_func = [this, func, context]( const operation_notification& o )
  {
    std::string name;

    if (_benchmark_dumper.is_enabled())
    {
      name = o.op.get_stored_type_name();
      _benchmark_dumper.begin();
    }

    func( o );

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( context, name );
  };

  return hive::utilities::make_signal_connection_ptr( _my->_post_apply_operation_signal.connect(group, complex_func) );
}

database::signal_connection_ptr database::add_pre_apply_transaction_handler( const apply_transaction_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<true>(_my->_pre_apply_transaction_signal, func, _benchmark_dumper, plugin, group, "transaction");
}

database::signal_connection_ptr database::add_post_apply_transaction_handler( const apply_transaction_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<false>(_my->_post_apply_transaction_signal, func, _benchmark_dumper, plugin, group, "transaction");
}

database::signal_connection_ptr database::add_pre_apply_custom_operation_handler ( const apply_custom_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl< true >(_my->_pre_apply_custom_operation_signal, func, _benchmark_dumper, plugin, group, "custom");
}

database::signal_connection_ptr database::add_post_apply_custom_operation_handler( const apply_custom_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl< false >(_my->_post_apply_custom_operation_signal, func, _benchmark_dumper, plugin, group, "custom");
}

database::signal_connection_ptr database::add_pre_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<true>(_my->_pre_apply_block_signal, func, _benchmark_dumper, plugin, group, "block");
}

database::signal_connection_ptr database::add_post_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<false>(_my->_post_apply_block_signal, func, _benchmark_dumper, plugin, group, "block");
}

database::signal_connection_ptr database::add_fail_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<false>(_my->_fail_apply_block_signal, func, _benchmark_dumper, plugin, group, "failed block");
}

database::signal_connection_ptr database::add_irreversible_block_handler( const irreversible_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<false>(_my->_on_irreversible_block, func, _benchmark_dumper, plugin, group, "irreversible");
}

database::signal_connection_ptr database::add_switch_fork_handler( const switch_fork_handler_t& func,
                                                                      const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<false>(_my->_switch_fork_signal, func, _benchmark_dumper, plugin, group, "switch_fork");
}

database::signal_connection_ptr database::add_pre_reindex_handler(const reindex_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<true>(_my->_pre_reindex_signal, func, _benchmark_dumper, plugin, group, "reindex");
}

database::signal_connection_ptr database::add_post_reindex_handler(const reindex_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<false>(_my->_post_reindex_signal, func, _benchmark_dumper, plugin, group, "reindex");
}

database::signal_connection_ptr database::add_finish_push_block_handler( const push_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<false>(_my->_finish_push_block_signal, func, _benchmark_dumper, plugin, group, "block");
}

database::signal_connection_ptr database::add_prepare_snapshot_handler(const prepare_snapshot_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_signal_impl<true>(_my->_prepare_snapshot_signal, func, _benchmark_dumper, plugin, group, "prepare_snapshot");
}

database::signal_connection_ptr database::add_snapshot_supplement_handler(const prepare_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_signal_impl<true>(_my->_prepare_snapshot_supplement_signal, func, _benchmark_dumper, plugin, group, "prepare_snapshot_data_supplement");
}

database::signal_connection_ptr database::add_snapshot_supplement_handler(const load_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_signal_impl<true>(_my->_load_snapshot_supplement_signal, func, _benchmark_dumper, plugin, group, "load_snapshot_data_supplement");
}

database::signal_connection_ptr database::add_comment_reward_handler(const comment_reward_notification_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_signal_impl<true>(_my->_comment_reward_signal, func, _benchmark_dumper, plugin, group, "comment_reward");
}

database::signal_connection_ptr database::add_end_of_syncing_handler(const end_of_syncing_notification_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_signal_impl<false>(_my->_end_of_syncing_signal, func, _benchmark_dumper, plugin, group, "->syncing_end");
}

database::signal_connection_ptr database::add_wipe_handler(const wipe_notification_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_signal_impl<false>(_my->_wipe_signal, func, _benchmark_dumper, plugin, group, "wipe storages");
}

database::signal_connection_ptr database::add_flush_handler( const flush_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_signal_impl<false>(_my->_flush_signal, func, _benchmark_dumper, plugin, group, "flush");
}

database::signal_connection_ptr database::add_plugin_index_handler( const std::function<void()>& func )
{
  return hive::utilities::make_signal_connection_ptr( _my->_plugin_index_signal.connect( func ) );
}

} } // hive::chain
