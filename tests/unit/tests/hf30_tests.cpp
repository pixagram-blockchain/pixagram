#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/detail/state/hardfork_property_object.hpp>
#include <hive/chain/hardfork_property_object_multiindex.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>
#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/comment_object_multiindex.hpp>
#include <hive/chain/util/dhf_funding.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/protocol/legacy_asset.hpp>
#include <hive/chain/rc/resource_count.hpp>
#include "../db_fixture/clean_database_fixture.hpp"

#include <map>

using namespace hive::chain;
using namespace hive::protocol;

namespace
{
struct hf30_database_fixture : hardfork_database_fixture
{
  explicit hf30_database_fixture( uint32_t witnesses = 9 )
    : hardfork_database_fixture( database_fixture::shared_file_size_small, HIVE_HARDFORK_1_29, witnesses )
  {
    generate_block();
  }

  // Give every witness approval above the HF30 floor so activation keeps the schedule at its size.
  void approve_all_witnesses()
  {
    const int64_t total = db->get_dynamic_global_properties().total_vesting_shares.amount.value;
    const int64_t minimum = total / 100 + ( total % 100 != 0 );
    db_plugin->debug_update( [=]( database& chain )
    {
      for( const auto& witness : chain.get_index< witness_index, by_id >() )
        chain.modify( witness, [&]( witness_object& w ) { w.votes = share_type( 2 * minimum ); } );
    } );
    generate_block();
  }

  void activate()
  {
    // A direct set_hardfork in a pending session is undone by generate_block.
    // Replay this activation in the debug transaction committed by that block.
    db_plugin->debug_update( []( database& chain ) { chain.set_hardfork( HIVE_HARDFORK_1_30 ); } );
    generate_block();
    BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_30 ) );
  }
};

struct hf30_test_schedule
{
  const testnet_blockchain_configuration::configuration saved = configuration_data;

  hf30_test_schedule()
  {
    configuration_data.set_hardfork_schedule( HIVE_GENESIS_TIME,
      { { HIVE_HARDFORK_1_29, 0 }, { HIVE_HARDFORK_1_30, 100 } } );
    // Put the inherited owner-history tracking block out of reach of this test, so an owner change
    // is recorded only because HF30 turned tracking on, not because the block number passed it.
    configuration_data.set_owner_auth_history_tracking_start_block( 1'000'000 );
  }

  ~hf30_test_schedule() { configuration_data = saved; }
};

struct hf30_activation_fixture : hf30_test_schedule, hf30_database_fixture {};
}

BOOST_FIXTURE_TEST_SUITE( hf30_tests, hf30_database_fixture )

BOOST_AUTO_TEST_CASE( disabled_witness_does_not_halt_pre_hf30_schedule )
{ try {
  account_create( "disabled", init_account_pub_key );
  fund( "disabled", HIVE_MIN_PRODUCER_REWARD );
  witness_create( "disabled", init_account_priv_key, "foo.bar", public_key_type(), 0 );
  generate_blocks( 2 * HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( uint32_t( db->get_witness_schedule_object().num_scheduled_witnesses ), 9u );
  BOOST_REQUIRE_EQUAL( uint32_t( db->get_future_witness_schedule_object().num_scheduled_witnesses ), 9u );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( unapproved_witnesses_cannot_expand_schedule )
{ try {
  // Give the nine existing witnesses approval above the one-percent floor.
  const int64_t total = db->get_dynamic_global_properties().total_vesting_shares.amount.value;
  const int64_t minimum = total / 100 + ( total % 100 != 0 );
  db_plugin->debug_update( [=]( database& chain )
  {
    for( const auto& witness : chain.get_index< witness_index, by_id >() )
      chain.modify( witness, [&]( witness_object& w ) { w.votes = share_type( 2 * minimum ); } );
  } );
  for( unsigned i = 0; i < 4; ++i )
  {
    const auto name = std::string( "sybil" ) + std::to_string( i );
    account_create( name, init_account_pub_key );
    fund( name, HIVE_MIN_PRODUCER_REWARD );
    witness_create( name, init_account_priv_key, "foo.bar", init_account_pub_key, 0 );
  }
  activate();
  generate_blocks( 3 * HIVE_MAX_WITNESSES );
  for( const auto* schedule : { &db->get_witness_schedule_object(), &db->get_future_witness_schedule_object() } )
  {
    BOOST_REQUIRE_EQUAL( uint32_t( schedule->num_scheduled_witnesses ), 9u );
    for( uint32_t i = 0; i < schedule->num_scheduled_witnesses; ++i )
      BOOST_REQUIRE( std::string( schedule->current_shuffled_witnesses[ i ] ).find( "sybil" ) != 0 );
  }
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( unapproved_witnesses_leave_schedule_after_activation )
{ try {
  approve_all_witnesses();
  // Four witnesses with valid keys but no approval. Before HF30 they fill empty schedule slots.
  for( unsigned i = 0; i < 4; ++i )
  {
    const auto name = std::string( "late" ) + std::to_string( i );
    account_create( name, init_account_pub_key );
    fund( name, HIVE_MIN_PRODUCER_REWARD );
    witness_create( name, init_account_priv_key, "foo.bar", init_account_pub_key, 0 );
  }
  generate_blocks( 2 * HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( uint32_t( db->get_witness_schedule_object().num_scheduled_witnesses ), 13u );

  activate();
  generate_blocks( 3 * HIVE_MAX_WITNESSES ); // a few rotations for the stale schedule to clear

  for( const auto* schedule : { &db->get_witness_schedule_object(), &db->get_future_witness_schedule_object() } )
  {
    BOOST_REQUIRE_EQUAL( uint32_t( schedule->num_scheduled_witnesses ), 9u );
    for( uint32_t i = 0; i < schedule->num_scheduled_witnesses; ++i )
      BOOST_REQUIRE( std::string( schedule->current_shuffled_witnesses[ i ] ).find( "late" ) != 0 );
  }
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( open_authority_account_cannot_operate_witness_after_hf30 )
{ try {
  // temp is a genesis system account whose active authority has a zero weight threshold, so an
  // unsigned transaction satisfies it. Before HF30 that lets anyone register it as a witness; from
  // HF30 the witness operation is rejected.
  const uint32_t temp_active_threshold =
    db->get< account_authority_object, by_account >( HIVE_TEMP_ACCOUNT ).active.weight_threshold;
  BOOST_REQUIRE_EQUAL( temp_active_threshold, 0u );

  auto unsigned_update = [&]()
  {
    witness_update_operation op;
    op.owner = HIVE_TEMP_ACCOUNT;
    op.url = "foo.bar";
    op.block_signing_key = generate_private_key( "temp_wit" ).get_public_key();
    op.fee = ASSET( "0.000 TESTS" );
    op.props.account_creation_fee = legacy_hive_asset::from_asset( ASSET( "1.000 TESTS" ) );
    op.props.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT + 1;
    op.props.hbd_interest_rate = 0;
    signed_transaction trx;
    trx.operations.push_back( op );
    trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    return trx;
  };

  push_transaction( unsigned_update(), fc::ecc::private_key() ); // no signature; accepted pre-HF30
  generate_block();
  const witness_object* temp_witness = db->find< witness_object, by_name >( HIVE_TEMP_ACCOUNT );
  BOOST_REQUIRE( temp_witness != nullptr );

  activate();

  HIVE_REQUIRE_THROW( push_transaction( unsigned_update(), fc::ecc::private_key() ), fc::assert_exception );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( last_enabled_witness_cannot_disable_itself )
{ try {
  // One producer is sufficient to exercise both update paths without changing votes.
  for( uint32_t i = 1; i < 9; ++i )
  {
    const auto name = HIVE_INIT_MINER_NAME + fc::to_string( i );
    witness_create( name, init_account_priv_key, "foo.bar", public_key_type(), 0 );
  }
  activate();
  HIVE_REQUIRE_THROW( witness_create( HIVE_INIT_MINER_NAME, init_account_priv_key,
    "foo.bar", public_key_type(), 0 ), fc::assert_exception );

  witness_set_properties_operation op;
  op.owner = HIVE_INIT_MINER_NAME;
  op.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );
  op.props[ "new_signing_key" ] = fc::raw::pack_to_vector( public_key_type() );
  HIVE_REQUIRE_THROW( push_transaction( op, init_account_priv_key ), fc::assert_exception );
  BOOST_REQUIRE( !db->get_witness( HIVE_INIT_MINER_NAME ).is_disabled() );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( witness_vote_cannot_advance_hf30_activation_time )
{ try {
  const auto wrong_time = db->head_block_time();
  db_plugin->debug_update( [=]( database& chain )
  {
    chain.modify( chain.get_hardfork_property_object(), [&]( hardfork_property_object& hpo )
    {
      hpo.next_hardfork = HIVE_HARDFORK_1_30_VERSION;
      hpo.next_hardfork_time = wrong_time;
    } );
  } );
  generate_block();
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_30 ) );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( small_account_votes_count_from_hf30 )
{ try {
  ACTORS( (author)(smallvoter) )
  // About 100 VESTS: a full vote spends 2% of it, 2,000,000 rshares, which the inherited
  // 50,000,000 floor wipes out and the HF30 floor of 50,000 does not.
  const auto& dgp = db->get_dynamic_global_properties();
  const int64_t pixa = fc::uint128_to_int64( fc::uint128_t( 100'000'000 ) * dgp.total_vesting_fund_hive.amount.value
    / dgp.total_vesting_shares.amount.value );
  vest( "author", HIVE_asset( 3'000'000 ) );   // resource credits for the post
  vest( "smallvoter", HIVE_asset( pixa ) );
  generate_block();
  const int64_t vests = db->get_account( "smallvoter" ).get_vesting().amount.value;
  BOOST_REQUIRE_GT( vests, 2'500'000 );          // above the HF30 floor for a full vote (2.5 VESTS)
  BOOST_REQUIRE_LT( vests, 2'500'000'000 );      // below the inherited floor (2,500 VESTS)

  post_comment( "author", "post", "title", "body", "test", author_post_key );
  auto rshares = [&]()
  {
    const auto post_id = db->get_comment( "author", std::string( "post" ) )->get_id();
    return db->get< comment_vote_object, by_comment_voter >(
      boost::make_tuple( post_id, db->get_account( "smallvoter" ).get_id() ) ).get_rshares();
  };

  vote( "author", "post", "smallvoter", HIVE_100_PERCENT, smallvoter_post_key );
  BOOST_REQUIRE_EQUAL( rshares(), 0 );

  approve_all_witnesses();
  activate();
  vote( "author", "post", "smallvoter", 90 * HIVE_1_PERCENT, smallvoter_post_key );
  BOOST_REQUIRE_GT( rshares(), 0 );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( partial_schedule_pays_the_nominal_witness_share )
{ try {
  auto producer_reward = [&]() -> int64_t
  {
    std::map< std::string, int64_t > before;
    for( const auto& witness : db->get_index< witness_index, by_id >() )
      before[ std::string( witness.owner ) ] = db->get_account( witness.owner ).get_vesting().amount.value;
    generate_block();
    const std::string producer( db->get_dynamic_global_properties().current_witness );
    return db->get_account( producer ).get_vesting().amount.value - before[ producer ];
  };

  approve_all_witnesses();
  generate_blocks( 2 * HIVE_MAX_WITNESSES ); // let the schedule settle to the nine approved witnesses
  BOOST_REQUIRE_EQUAL( uint32_t( db->get_witness_schedule_object().num_scheduled_witnesses ), 9u );
  const int64_t before_hf30 = producer_reward();
  activate();
  generate_blocks( 2 * HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( uint32_t( db->get_witness_schedule_object().num_scheduled_witnesses ), 9u );
  const int64_t after_hf30 = producer_reward();

  // With nine witnesses every block used to pay 21/9 of the nominal share; from HF30 it pays one.
  // Check the ~2.33x drop with a range that tolerates per-block integer rounding.
  BOOST_REQUIRE_GT( after_hf30, 0 );
  BOOST_REQUIRE_GT( before_hf30, 2 * after_hf30 );
  BOOST_REQUIRE_LT( before_hf30, 2.6 * after_hf30 );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( dhf_funding_keeps_the_sub_milli_remainder )
{ try {
  // Mainnet today: 0.140 PIXA of DPF share per block at 1 PXS = 51.833 PIXA is 0.0027 PXS a block.
  const HBD_price median( HBD_asset( 1000 ), HIVE_asset( 51833 ) );
  const HIVE_asset share( 140 );
  int64_t truncated = 0, exact = 0;
  for( uint32_t block = 1000; block < 2000; ++block )
  {
    truncated += ( share * median ).get_amount();   // the pre-HF30 conversion
    const int64_t paid = util::dhf_funding_without_truncation( share, median, block ).get_amount();
    BOOST_REQUIRE( paid == 2 || paid == 3 );     // each block pays floor or ceil of 2.70 milli-PXS
    exact += paid;
  }
  BOOST_REQUIRE_EQUAL( truncated, 2000 );        // 0.002 PXS a block: 26% of the share lost
  // floor(2000 * 2.700982) - floor(1000 * 2.700982) = 5401 - 2700
  BOOST_REQUIRE_EQUAL( exact, 2701 );
  BOOST_REQUIRE_EQUAL( util::dhf_funding_without_truncation( HIVE_asset( 0 ), median, 5 ).get_amount(), 0 );

  // Past the rounding cliff: once the per-block share falls below 0.001 PXS the pre-HF30
  // conversion pays the treasury NOTHING, for every block, permanently. At today's 0.140 PIXA
  // share that happens above ~140.2 PIXA per PXS. HF30 must still deliver the exact amount by
  // accumulating the sub-milli remainder across blocks.
  for( const auto quote : { 200, 2000 } )
  {
    const HBD_price weak( HBD_asset( 1000 ), HIVE_asset( share.get_amount() * 1000 / quote * quote ) );
    const HBD_price cliff( HBD_asset( 1000 ), HIVE_asset( quote * 1000 ) );
    (void) weak;
    int64_t old_total = 0, new_total = 0;
    for( uint32_t block = 1000; block < 11000; ++block )
    {
      old_total += ( share * cliff ).get_amount();
      new_total += util::dhf_funding_without_truncation( share, cliff, block ).get_amount();
    }
    // every single block truncates to zero before HF30
    BOOST_REQUIRE_EQUAL( old_total, 0 );
    // HF30 pays the exact share over the window (within one unit of boundary rounding)
    const int64_t exact = int64_t( share.get_amount() ) * 1000 * 10000 / ( int64_t( quote ) * 1000 );
    BOOST_REQUIRE_GE( new_total, exact - 1 );
    BOOST_REQUIRE_LE( new_total, exact + 1 );
    BOOST_REQUIRE_GT( new_total, 0 );
  }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( custom_json_rc_scales_with_payload_length_after_hf30 )
{ try {
  // Pixagram raised HIVE_CUSTOM_OP_DATA_MAX_LENGTH from 8 KiB to 64 KiB but kept charging a flat
  // resource-credit cost, so a maximum-size payload cost no more than a tiny one despite eight
  // times the UTF-8 validation and JSON parsing. From HF30 the cost scales with length.
  auto cost_of = [&]( size_t json_bytes, bool after_hf30 )
  {
    custom_json_operation op;
    op.required_posting_auths.insert( "alice" );
    op.id = "test";
    op.json = "[\"x\",{\"d\":\"" + std::string( json_bytes, 'a' ) + "\"}]";
    signed_transaction trx;
    trx.operations.push_back( op );
    count_resources_result usage;
    count_resources( trx, 1000, usage, db->head_block_time(), after_hf30 );
    return usage[ resource_execution_time ];
  };

  const int64_t small_before = cost_of( 100, false );
  const int64_t large_before = cost_of( 60000, false );
  const int64_t small_after  = cost_of( 100, true );
  const int64_t large_after  = cost_of( 60000, true );

  // Before HF30 a 60 KB payload costs exactly the same as a 100-byte one: length is not priced.
  BOOST_REQUIRE_EQUAL( small_before, large_before );
  // After HF30 both carry the same per-transaction base cost, so compare what the payload itself
  // adds. The large payload must add far more than the small one.
  const int64_t small_added = small_after - small_before;
  const int64_t large_added = large_after - large_before;
  BOOST_REQUIRE_GT( large_added, 0 );
  BOOST_REQUIRE_GT( large_added, small_added * 100 );
  BOOST_REQUIRE_GT( large_after, large_before );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( small_proposal_is_paid_instead_of_truncated_to_zero )
{ try {
  // A proposal's hourly slice used to be floor( elapsed * daily_pay / 86400 ) on its own, so the
  // part under 0.001 PXS was discarded every period and a proposal paying less than 0.024 PXS a day
  // received exactly nothing, for ever. HF30 pays the difference of two running totals from the
  // proposal's start, so the remainder carries. Model both here over a day of hourly periods.
  const int64_t day = 86400, hour = 3600;
  auto old_per_period = []( int64_t daily_pay, int64_t elapsed ) { return elapsed * daily_pay / 86400; };
  auto hf30_total = []( int64_t daily_pay, int64_t elapsed_now, int64_t elapsed_prev )
  {
    return ( elapsed_now * daily_pay ) / 86400 - ( elapsed_prev * daily_pay ) / 86400;
  };

  for( const int64_t daily_pay : { int64_t( 20 ), int64_t( 10 ), int64_t( 110000 ) } ) // 0.020, 0.010, 110.000 PXS/day
  {
    int64_t old_total = 0, new_total = 0;
    for( int64_t h = 0; h < day / hour; ++h )
    {
      old_total += old_per_period( daily_pay, hour );
      new_total += hf30_total( daily_pay, ( h + 1 ) * hour, h * hour );
    }
    // HF30 always pays the exact daily amount
    BOOST_REQUIRE_EQUAL( new_total, daily_pay );
    if( daily_pay < 24 )
      BOOST_REQUIRE_EQUAL( old_total, 0 );   // below 0.024 PXS/day the old path paid nothing at all
    BOOST_REQUIRE_GE( new_total, old_total );
  }
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( reward_conversion_does_not_burn_pixa_after_hf30 )
{ try {
  // Converting the PXS half of a reward truncates to 0.001 PXS. Before HF30 the whole PIXA amount
  // was burned regardless, destroying the remainder - and destroying the entire PXS half when the
  // reward was small enough to round to zero. Verify the remainder is now returned as PIXA.
  ACTORS( (convauthor) )
  vest( "convauthor", HIVE_asset( 10'000'000 ) );
  approve_all_witnesses();
  activate();
  generate_block();

  // The testnet fixture's median is 1:1, where nothing ever truncates. Skew it to mainnet's ratio
  // (1 PXS = 51.833 PIXA) so the conversion actually rounds, which is the condition being tested.
  db_plugin->debug_update( []( database& chain )
  {
    chain.modify( chain.get_feed_history(), []( feed_history_object& f )
    {
      f.current_median_history = HBD_price( HBD_asset( 1000 ), HIVE_asset( 51833 ) );
    } );
  } );
  generate_block();
  const auto median = db->get_feed_history().current_median_history;
  BOOST_REQUIRE( !median.is_null() );

  // An amount small enough that the PXS leg truncates: below one whole 0.001 PXS it used to vanish.
  const HIVE_asset tiny( 10 ); // 0.010 PIXA, well under the ~0.052 PIXA needed for 1 PXS satoshi
  const HBD_asset as_pxs = tiny * median;
  BOOST_REQUIRE_EQUAL( as_pxs.get_amount(), 0 ); // rounds to zero, so the old path burned all of it

  const auto balance_before = db->get_balance( db->get_account( "convauthor" ), HIVE_SYMBOL );
  const auto pxs_before = db->get_balance( db->get_account( "convauthor" ), HBD_SYMBOL );
  db_plugin->debug_update( [=]( database& chain )
  {
    // create_hbd assumes its caller already minted the PIXA being converted (reward payouts do),
    // so mint it here too - otherwise the chain's supply invariant is violated by the test setup
    // rather than by the code under test.
    chain.adjust_supply( tiny );
    chain.create_hbd( chain.get_account( "convauthor" ), tiny, false );
  } );
  generate_block();
  const auto balance_after = db->get_balance( db->get_account( "convauthor" ), HIVE_SYMBOL );
  const auto pxs_after = db->get_balance( db->get_account( "convauthor" ), HBD_SYMBOL );

  // Nothing was destroyed: the PIXA that could not be converted came back to the account. Before
  // HF30 the whole amount was burned (print rate 10000 sends it all to the PXS leg, which rounds to
  // zero), so the account would have received nothing at all and this delta would be 0.
  // Total supply is deliberately not compared: generating a block mints the block reward, which
  // would swamp the amount under test.
  BOOST_REQUIRE_EQUAL( ( balance_after - balance_before ).amount.value, tiny.get_amount() );
  BOOST_REQUIRE_EQUAL( ( pxs_after - pxs_before ).amount.value, 0 ); // nothing minted as PXS
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( treasury_receives_the_full_dhf_share_after_hf30 )
{ try {
  approve_all_witnesses();
  activate();
  const auto& treasury = db->get_treasury();
  const int64_t start = treasury.get_hbd_balance().amount.value;
  generate_blocks( 200 );
  BOOST_REQUIRE_GT( db->get_treasury().get_hbd_balance().amount.value, start );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( non_producing_witnesses_do_not_raise_hardfork_quorum )
{ try {
  // Audit finding #2: the hardfork-vote quorum must be based on witnesses that actually PRODUCE, not
  // on every scheduled witness. A non-producing witness keeps the default (0.0.0) hardfork vote that
  // the stale-vote filter ignores, so it raises the quorum denominator but can never vote toward it.
  // An attacker who registers free, non-producing witnesses could otherwise push the quorum above
  // what the honest producers can reach and permanently block HF30 - and every future hardfork -
  // from activating by vote. The fix is deliberately NOT hardfork-gated, so it is exercised here at
  // HF29, before HF30 activates.
  //
  // The witness-schedule tally recomputes next_hardfork from scratch on every schedule boundary: it
  // either sets it to a version+time tuple that reaches the quorum, or clears it to the current
  // version when none does. So after a boundary, next_hardfork == 1.30 means THIS boundary's tally
  // found the 1.30 vote reaching quorum. With only the nine honest producers voting 1.30, that is
  // possible only when the denominator is the producing count (fix) and not num_scheduled (no fix).
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_30 ) );

  // The nine fixture witnesses are the honest producers. Add four attackers with valid keys but no
  // stake: they fill empty schedule slots (13 <= HIVE_MAX_WITNESSES) and so inflate num_scheduled.
  for( unsigned i = 0; i < 4; ++i )
  {
    const auto name = std::string( "attacker" ) + std::to_string( i );
    account_create( name, init_account_pub_key );
    fund( name, HIVE_MIN_PRODUCER_REWARD );
    witness_create( name, init_account_priv_key, "foo.bar", init_account_pub_key, 0 );
  }
  // Let the schedule settle so all thirteen witnesses are scheduled and each has produced recently.
  generate_blocks( 2 * HIVE_MAX_WITNESSES );
  const uint32_t num_scheduled = db->get_future_witness_schedule_object().num_scheduled_witnesses;
  BOOST_REQUIRE_EQUAL( num_scheduled, 13u );

  const auto activation_time = db->get_hardfork_versions().times[ HIVE_HARDFORK_1_30 ];
  const auto is_attacker = []( const account_name_type& name )
  {
    return std::string( name ).rfind( "attacker", 0 ) == 0;
  };
  // Land the next schedule recompute on the following % HIVE_MAX_WITNESSES boundary, so a debug_update
  // - which takes effect for exactly one block - is in force when that boundary's tally runs.
  auto advance_to_boundary = [&]()
  {
    while( ( db->head_block_num() + 1 ) % HIVE_MAX_WITNESSES != 0 )
      generate_block();
  };

  // Clean baseline: make every witness vote a version the tally ignores (0.0.0 <= current), so the
  // next boundary clears next_hardfork. This shows the flip below is produced by that later tally,
  // not left over from the fixture's genesis votes.
  advance_to_boundary();
  db_plugin->debug_update( []( database& chain )
  {
    for( const auto& witness : chain.get_index< witness_index, by_id >() )
      chain.modify( witness, [&]( witness_object& w )
      {
        w.hardfork_version_vote = hardfork_version();
        w.hardfork_time_vote = fc::time_point_sec();
      } );
  } );
  generate_block();
  BOOST_REQUIRE_EQUAL( db->head_block_num() % HIVE_MAX_WITNESSES, 0u );
  BOOST_REQUIRE( db->get_hardfork_property_object().next_hardfork != HIVE_HARDFORK_1_30_VERSION );

  // Decisive boundary: the nine honest producers cast a real (1.30, activation_time) vote. The four
  // attackers keep an ignored vote AND are marked as never having produced (last_confirmed_block_num
  // == 0) - exactly the on-chain footprint of a registered-but-idle witness, which the test's own
  // block production would otherwise keep refreshing.
  advance_to_boundary();
  db_plugin->debug_update( [=]( database& chain )
  {
    for( const auto& witness : chain.get_index< witness_index, by_id >() )
    {
      if( is_attacker( witness.owner ) )
        chain.modify( witness, [&]( witness_object& w )
        {
          w.hardfork_version_vote = hardfork_version();
          w.hardfork_time_vote = fc::time_point_sec();
          w.last_confirmed_block_num = 0;
        } );
      else
        chain.modify( witness, [&]( witness_object& w )
        {
          w.hardfork_version_vote = HIVE_HARDFORK_1_30_VERSION;
          w.hardfork_time_vote = activation_time;
        } );
    }
  } );
  generate_block();
  BOOST_REQUIRE_EQUAL( db->head_block_num() % HIVE_MAX_WITNESSES, 0u );

  // (a) With the fix the quorum denominator is the nine producers, so the real vote reaches quorum
  // and the hardfork becomes pending despite the four idle witnesses inflating num_scheduled.
  const auto& hpo = db->get_hardfork_property_object();
  const auto pending_hardfork = hpo.next_hardfork;
  const auto pending_hardfork_time = hpo.next_hardfork_time;
  BOOST_REQUIRE( pending_hardfork == HIVE_HARDFORK_1_30_VERSION );
  BOOST_REQUIRE( pending_hardfork_time == activation_time );

  // (b) Without the fix the denominator would be num_scheduled (13), whose quorum (11) exceeds the
  // nine honest votes, so this same boundary's tally would instead have cleared next_hardfork. Assign
  // the templated calls to locals first to avoid the BOOST macro-comma gotcha.
  uint32_t honest_votes = 0;
  for( const auto& witness : db->get_index< witness_index, by_id >() )
    if( !is_attacker( witness.owner ) )
      ++honest_votes;
  BOOST_REQUIRE_EQUAL( honest_votes, 9u );
  const uint32_t quorum_with_fix = pixa_hardfork_quorum( honest_votes );
  const uint32_t quorum_without_fix = pixa_hardfork_quorum( num_scheduled );
  BOOST_REQUIRE_EQUAL( quorum_with_fix, 8u );
  BOOST_REQUIRE_EQUAL( quorum_without_fix, 11u );
  BOOST_REQUIRE_GE( honest_votes, quorum_with_fix );     // reachable with the fix
  BOOST_REQUIRE_LT( honest_votes, quorum_without_fix );  // unreachable without it
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( hf30_activation_tests, hf30_activation_fixture )

BOOST_AUTO_TEST_CASE( exact_witness_vote_activates_and_owner_history_is_recorded )
{ try {
  const auto activation_time = db->get_hardfork_versions().times[ HIVE_HARDFORK_1_30 ];
  BOOST_REQUIRE_EQUAL( activation_time.sec_since_epoch(),
    HIVE_GENESIS_TIME.sec_since_epoch() + 100 * HIVE_BLOCK_INTERVAL );

  db_plugin->debug_update( [=]( database& chain )
  {
    for( const auto& witness : chain.get_index< witness_index, by_id >() )
      chain.modify( witness, [&]( witness_object& w )
      {
        w.hardfork_version_vote = HIVE_HARDFORK_1_30_VERSION;
        w.hardfork_time_vote = activation_time;
      } );
  } );
  generate_blocks( 2 * HIVE_MAX_WITNESSES );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_30 ) );
  BOOST_REQUIRE( db->get_hardfork_property_object().next_hardfork == HIVE_HARDFORK_1_30_VERSION );
  BOOST_REQUIRE( db->get_hardfork_property_object().next_hardfork_time == activation_time );

  // Before HF30, and far below the tracking block, an owner change records no history.
  BOOST_REQUIRE_LT( db->head_block_num(), 1'000'000u );
  account_create( "early", init_account_pub_key );
  generate_block();
  db_plugin->debug_update( [=]( database& chain )
  {
    chain.update_owner_authority( chain.get_account( "early" ),
      authority( 1, generate_private_key( "hf30_early" ).get_public_key(), 1 ) );
  } );
  generate_block();
  {
    const auto& h = db->get_index< owner_authority_history_index, by_account >();
    BOOST_REQUIRE( h.lower_bound( account_name_type( "early" ) ) == h.end()
      || h.lower_bound( account_name_type( "early" ) )->account != account_name_type( "early" ) );
  }

  generate_blocks( activation_time + fc::seconds( HIVE_BLOCK_INTERVAL ) );
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_30 ) );
  BOOST_REQUIRE_LT( db->head_block_num(), 1'000'000u ); // still far below the inherited tracking block

  account_create( "recoverable", init_account_pub_key );
  generate_block();
  const authority previous_owner( 1, init_account_pub_key, 1 );
  const auto replacement_key = generate_private_key( "hf30_new_owner" ).get_public_key();
  db_plugin->debug_update( [=]( database& chain )
  {
    chain.update_owner_authority( chain.get_account( "recoverable" ), authority( 1, replacement_key, 1 ) );
  } );
  generate_block();
  const auto& history = db->get_index< owner_authority_history_index, by_account >();
  const auto found = history.lower_bound( account_name_type( "recoverable" ) );
  BOOST_REQUIRE( found != history.end() );
  BOOST_REQUIRE( found->account == account_name_type( "recoverable" ) );
  BOOST_REQUIRE( found->previous_owner_authority == previous_owner );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
#endif
