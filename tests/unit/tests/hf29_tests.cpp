#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/detail/state/hardfork_property_object.hpp>
#include <hive/chain/hardfork_property_object_multiindex.hpp>
#include <hive/chain/detail/state/reward_fund_object.hpp>
#include <hive/chain/detail/state/reward_fund_object_multiindex.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

namespace
{

/// Starts the chain at HF28 - where mainnet sits while HF29 is being voted in.
struct hf29_database_fixture : public hardfork_database_fixture
{
  explicit hf29_database_fixture( uint32_t num_witnesses = HIVE_MAX_WITNESSES )
    : hardfork_database_fixture( database_fixture::shared_file_size_small, HIVE_HARDFORK_1_28, num_witnesses )
  {
    // Commit the accounts/witnesses left pending by the base fixture. Otherwise a direct
    // set_hardfork() below is silently undone when generate_block() resets that session.
    generate_block();
    BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_28 ) );
    BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  }

  const reward_fund_object& post_reward_fund() const
  {
    return db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );
  }

  void set_recent_claims( const fc::uint128_t& value )
  {
    db->modify( post_reward_fund(), [&]( reward_fund_object& rfo ) { rfo.recent_claims = value; } );
  }

  /// Overwrites every witness's stored hardfork vote - what stale state looks like. Goes through
  /// the debug node so the change survives the pending-session reset of the next generated block.
  void vote_all( const hardfork_version& v, const fc::time_point_sec& t )
  {
    db_plugin->debug_update( [=]( database& db )
    {
      std::vector< const witness_object* > witnesses;
      for( const auto& w : db.get_index< witness_index >().indices().get< by_id >() )
        witnesses.push_back( &w );
      for( const auto* w : witnesses )
        db.modify( *w, [&]( witness_object& wo )
        {
          wo.hardfork_version_vote = v;
          wo.hardfork_time_vote = t;
        } );
    } );
  }

  /// Generates blocks up to, but not including, the next block on which the witness schedule
  /// is updated and hardfork votes are tallied.
  void generate_until_before_schedule_update()
  {
    const uint32_t remainder = db->head_block_num() % HIVE_MAX_WITNESSES;
    generate_blocks( HIVE_MAX_WITNESSES - remainder - 1 );
    BOOST_REQUIRE_EQUAL( ( db->head_block_num() + 1 ) % HIVE_MAX_WITNESSES, 0u );
  }
};

// Configure a short test-only activation time before the database opens. Restore the whole
// configuration after database teardown, including when a test assertion throws.
struct hf29_test_schedule
{
  const configuration saved_configuration = configuration_data;

  hf29_test_schedule()
  {
    configuration_data.set_hardfork_schedule( HIVE_GENESIS_TIME,
      { { HIVE_HARDFORK_1_28, 0 }, { HIVE_HARDFORK_1_29, 2400 } } );
  }

  ~hf29_test_schedule() { configuration_data = saved_configuration; }
};

struct hf29_eight_witness_fixture : hf29_test_schedule, hf29_database_fixture
{
  hf29_eight_witness_fixture() : hf29_database_fixture( 8 )
  {
    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    require_schedule( 8, HIVE_HARDFORK_REQUIRED_WITNESSES );
  }

  fc::time_point_sec activation_time() const
  {
    return db->get_hardfork_versions().times[ HIVE_HARDFORK_1_29 ];
  }

  void require_schedule( uint32_t count, uint32_t quorum ) const
  {
    for( const auto* schedule : { &db->get_witness_schedule_object(), &db->get_future_witness_schedule_object() } )
    {
      BOOST_REQUIRE_EQUAL( uint32_t( schedule->num_scheduled_witnesses ), count );
      BOOST_REQUIRE_EQUAL( uint32_t( schedule->hardfork_required_witnesses ), quorum );
    }
  }

  // Apply exact vote tuples after the producer's block-header vote, through the debug
  // transaction. The remaining witnesses vote HF28 unless a different HF29 time is given.
  void set_votes( uint32_t matching, fc::optional<fc::time_point_sec> other_time = {} )
  {
    const auto time = activation_time();
    db_plugin->debug_update( [=]( database& db )
    {
      uint32_t i = 0;
      for( const auto& w : db.get_index< witness_index, by_id >() )
      {
        const bool matches = i++ < matching;
        db.modify( w, [&]( witness_object& wo )
        {
          wo.hardfork_version_vote = matches || other_time.valid()
            ? HIVE_HARDFORK_1_29_VERSION : HIVE_HARDFORK_1_28_VERSION;
          wo.hardfork_time_vote = matches ? time : other_time.value_or( time );
        } );
      }
      BOOST_REQUIRE_EQUAL( i, 8u );
    } );
  }

  void tally_votes( uint32_t matching, fc::optional<fc::time_point_sec> other_time = {} )
  {
    generate_until_before_schedule_update();
    set_votes( matching, other_time );
    generate_block();
  }

  void generate_at( fc::time_point_sec time )
  {
    BOOST_REQUIRE( time > db->head_block_time() );
    const auto slot = db->get_slot_at_time( time );
    BOOST_REQUIRE_GT( slot, 0u );
    generate_block( 0, init_account_priv_key, slot - 1 );
    BOOST_REQUIRE_EQUAL( db->head_block_time(), time );
  }
};

}

BOOST_FIXTURE_TEST_SUITE( hf29_tests, hf29_database_fixture )

BOOST_AUTO_TEST_CASE( quorum_scales_with_scheduled_witnesses )
{
  BOOST_TEST_MESSAGE( "Testing: pixa_hardfork_quorum keeps upstream's 17 of 21 and scales it down with the schedule" );

  BOOST_REQUIRE_EQUAL( pixa_hardfork_quorum( HIVE_MAX_WITNESSES ), uint32_t( HIVE_HARDFORK_REQUIRED_WITNESSES ) );
  BOOST_REQUIRE_EQUAL( pixa_hardfork_quorum( 20 ), 17u );
  BOOST_REQUIRE_EQUAL( pixa_hardfork_quorum( 8 ), 7u );
  BOOST_REQUIRE_EQUAL( pixa_hardfork_quorum( 3 ), 3u );
  BOOST_REQUIRE_EQUAL( pixa_hardfork_quorum( 1 ), 1u );
  BOOST_REQUIRE_EQUAL( pixa_hardfork_quorum( 0 ), uint32_t( HIVE_HARDFORK_REQUIRED_WITNESSES ) );

  for( uint32_t n = 1; n <= HIVE_MAX_WITNESSES; ++n )
  {
    const uint32_t quorum = pixa_hardfork_quorum( n );
    BOOST_REQUIRE_GE( quorum, 1u );
    BOOST_REQUIRE_LE( quorum, n );
    // never a weaker majority than upstream's ratio
    BOOST_REQUIRE_GE( quorum * HIVE_MAX_WITNESSES, n * HIVE_HARDFORK_REQUIRED_WITNESSES );
  }
}

BOOST_AUTO_TEST_CASE( recent_claims_reset_from_mainnet_seed )
{ try {
  BOOST_TEST_MESSAGE( "Testing: HF29 brings a fund seeded with the Hive mainnet value down to PIXA_HF29_RECENT_CLAIMS" );

  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_28 ) );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );

  set_recent_claims( HIVE_HF21_CONVERGENT_LINEAR_RECENT_CLAIMS );
  BOOST_REQUIRE( post_reward_fund().recent_claims > PIXA_HF29_RECENT_CLAIMS );
  const auto balance_before = post_reward_fund().get_reward_balance();

  db->set_hardfork( HIVE_HARDFORK_1_29 );

  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims == PIXA_HF29_RECENT_CLAIMS );
  BOOST_REQUIRE_EQUAL( post_reward_fund().get_reward_balance(), balance_before );
  BOOST_REQUIRE_EQUAL( post_reward_fund().last_update, db->head_block_time() );

  const auto& hfp = db->get_hardfork_property_object();
  BOOST_REQUIRE( hfp.current_hardfork_version == HIVE_HARDFORK_1_29_VERSION );
  BOOST_REQUIRE_EQUAL( hfp.last_hardfork, uint32_t( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE_EQUAL( hfp.processed_hardforks.size(), size_t( HIVE_HARDFORK_1_29 + 1 ) );

  generate_block();
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims < PIXA_HF29_RECENT_CLAIMS );
  BOOST_REQUIRE( post_reward_fund().recent_claims > 0 );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( recent_claims_reset_never_raises )
{ try {
  BOOST_TEST_MESSAGE( "Testing: HF29 leaves a fund that is already below the target alone" );

  const fc::uint128_t below_target = PIXA_HF29_RECENT_CLAIMS / 2;
  set_recent_claims( below_target );
  const auto balance_before = post_reward_fund().get_reward_balance();

  db->set_hardfork( HIVE_HARDFORK_1_29 );

  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims == below_target );
  BOOST_REQUIRE_EQUAL( post_reward_fund().get_reward_balance(), balance_before );
  BOOST_REQUIRE_EQUAL( post_reward_fund().last_update, db->head_block_time() );
  generate_block();
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims <= below_target );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( recent_claims_reset_leaves_empty_fund_alone )
{ try {
  BOOST_TEST_MESSAGE( "Testing: HF29 does not seed an empty fund" );

  set_recent_claims( 0 );
  const auto balance_before = post_reward_fund().get_reward_balance();

  db->set_hardfork( HIVE_HARDFORK_1_29 );

  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims == 0 );
  BOOST_REQUIRE_EQUAL( post_reward_fund().get_reward_balance(), balance_before );
  BOOST_REQUIRE_EQUAL( post_reward_fund().last_update, db->head_block_time() );
  generate_block();
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims == 0 );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( stored_quorum_follows_the_schedule_after_hf29 )
{ try {
  BOOST_TEST_MESSAGE( "Testing: hardfork_required_witnesses is rewritten by HF29 and tracks the schedule from then on" );

  const auto& wso = db->get_witness_schedule_object();
  const auto& future_wso = db->get_future_witness_schedule_object();
  BOOST_REQUIRE_EQUAL( uint32_t( wso.hardfork_required_witnesses ), uint32_t( HIVE_HARDFORK_REQUIRED_WITNESSES ) );

  db->set_hardfork( HIVE_HARDFORK_1_29 );

  BOOST_REQUIRE_EQUAL( uint32_t( wso.hardfork_required_witnesses ), pixa_hardfork_quorum( wso.num_scheduled_witnesses ) );
  BOOST_REQUIRE_EQUAL( uint32_t( future_wso.hardfork_required_witnesses ), pixa_hardfork_quorum( future_wso.num_scheduled_witnesses ) );

  generate_blocks( 2 * HIVE_MAX_WITNESSES );

  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE_EQUAL( uint32_t( wso.hardfork_required_witnesses ), pixa_hardfork_quorum( wso.num_scheduled_witnesses ) );
  BOOST_REQUIRE_EQUAL( uint32_t( future_wso.hardfork_required_witnesses ), pixa_hardfork_quorum( future_wso.num_scheduled_witnesses ) );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( hardfork_vote_tally )
{ try {
  BOOST_TEST_MESSAGE( "Testing: votes for versions already applied are ignored, genuine votes schedule HF29" );

  const auto& hfp = db->get_hardfork_property_object();
  const auto& hf_versions = db->get_hardfork_versions();
  BOOST_REQUIRE_EQUAL( std::string( hfp.current_hardfork_version ), std::string( HIVE_HARDFORK_1_28_VERSION ) );

  // Stale votes for a version the chain has already passed never win, however many there are.
  // This is mainnet's (0.0.0, genesis) situation, one hardfork up. Only the block's own producer
  // carries a genuine vote by the time the tally runs.
  generate_until_before_schedule_update();
  vote_all( hardfork_version(), fc::time_point_sec( 1 ) );
  generate_block();
  BOOST_REQUIRE_EQUAL( std::string( hfp.next_hardfork ), std::string( HIVE_HARDFORK_1_28_VERSION ) );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );

  // The same holds for votes for the current version itself.
  generate_until_before_schedule_update();
  vote_all( HIVE_HARDFORK_1_28_VERSION, db->head_block_time() );
  generate_block();
  BOOST_REQUIRE_EQUAL( std::string( hfp.next_hardfork ), std::string( HIVE_HARDFORK_1_28_VERSION ) );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );

  // Genuine votes come from the block producer: a witness running this binary writes
  // (1.29.0, HIVE_HARDFORK_1_29_TIME) into its block header until its stored vote matches.
  // Two rounds guarantee every scheduled witness has produced and a tally has run since.
  generate_blocks( 2 * HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( std::string( hfp.next_hardfork ), std::string( HIVE_HARDFORK_1_29_VERSION ) );
  BOOST_REQUIRE_EQUAL( hfp.next_hardfork_time.sec_since_epoch(), hf_versions.times[ HIVE_HARDFORK_1_29 ].sec_since_epoch() );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) ); // scheduled, not yet due
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( hf29_activation_tests, hf29_eight_witness_fixture )

BOOST_AUTO_TEST_CASE( eight_witness_quorum_requires_matching_version_and_time )
{ try {
  const auto& hfp = db->get_hardfork_property_object();
  const auto majority_before = db->get_witness_schedule_object().majority_version;

  tally_votes( 6 );
  BOOST_REQUIRE( hfp.next_hardfork == HIVE_HARDFORK_1_28_VERSION );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );

  // Eight HF29 versions alone do not suffice: neither time tuple has seven votes.
  tally_votes( 6, activation_time() + fc::seconds( HIVE_BLOCK_INTERVAL ) );
  BOOST_REQUIRE( hfp.next_hardfork == HIVE_HARDFORK_1_28_VERSION );

  tally_votes( 7 );
  BOOST_REQUIRE( hfp.next_hardfork == HIVE_HARDFORK_1_29_VERSION );
  BOOST_REQUIRE_EQUAL( hfp.next_hardfork_time, activation_time() );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  require_schedule( 8, HIVE_HARDFORK_REQUIRED_WITNESSES ); // stored/API threshold is gated
  BOOST_REQUIRE( db->get_witness_schedule_object().majority_version == majority_before );

  tally_votes( 6 );
  BOOST_REQUIRE( hfp.next_hardfork == HIVE_HARDFORK_1_28_VERSION );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( activation_at_voted_time_and_no_quorum_ratchet )
{ try {
  tally_votes( 7 );
  generate_at( activation_time() - fc::seconds( HIVE_BLOCK_INTERVAL ) );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );

  // Persist the seed through block generation; activation must happen through normal
  // process_hardforks(), not the test-only set_hardfork() shortcut.
  db_plugin->debug_update( []( database& db )
  {
    db.modify( db.get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ),
      [&]( reward_fund_object& rf ) { rf.recent_claims = HIVE_HF21_CONVERGENT_LINEAR_RECENT_CLAIMS; } );
  } );
  generate_at( activation_time() );
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims == PIXA_HF29_RECENT_CLAIMS );
  BOOST_REQUIRE_EQUAL( post_reward_fund().last_update, activation_time() );
  BOOST_REQUIRE_EQUAL( db->get_hardfork_property_object().processed_hardforks.size(), size_t( 30 ) );
  require_schedule( 8, 7 );

  for( uint32_t round = 0; round < 3; ++round )
  {
    generate_blocks( HIVE_MAX_WITNESSES );
    require_schedule( 8, 7 );
  }
  BOOST_REQUIRE( db->get_witness_schedule_object().majority_version == HIVE_BLOCKCHAIN_VERSION );

  // The migration runs once, not as a permanent upper bound on recent_claims.
  set_recent_claims( PIXA_HF29_RECENT_CLAIMS * 2 );
  generate_block();
  BOOST_REQUIRE( post_reward_fund().recent_claims > PIXA_HF29_RECENT_CLAIMS );
  BOOST_REQUIRE_EQUAL( db->get_hardfork_property_object().processed_hardforks.size(), size_t( 30 ) );

  for( uint32_t i = 8; i < HIVE_MAX_WITNESSES; ++i )
  {
    const auto name = HIVE_INIT_MINER_NAME + fc::to_string( i );
    account_create( name, init_account_pub_key );
    fund( name, HIVE_MIN_PRODUCER_REWARD );
    witness_create( name, init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
  }
  generate_blocks( 3 * HIVE_MAX_WITNESSES );
  require_schedule( HIVE_MAX_WITNESSES, HIVE_HARDFORK_REQUIRED_WITNESSES );
  generate_blocks( 3 * HIVE_MAX_WITNESSES );
  require_schedule( HIVE_MAX_WITNESSES, HIVE_HARDFORK_REQUIRED_WITNESSES );
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( late_quorum_still_activates )
{ try {
  tally_votes( 6 );
  generate_at( activation_time() );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  tally_votes( 7 );
  BOOST_REQUIRE( db->head_block_time() > activation_time() );
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  require_schedule( 8, 7 );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( cashouts_before_at_and_after_activation )
{ try {
  ACTORS( (alice)(bob)(carol) )
  // Genuine votes above Pixagram's rshare dust threshold, within the fixture's supply.
  for( const auto& name : { "alice", "bob", "carol" } )
    vest( name, HIVE_asset( 3'000'000'000 ) );
  set_price_feed( HBD_price( 1000, 1000 ), true );
  tally_votes( 7 );

  const auto post_time = activation_time() - fc::seconds( HIVE_CASHOUT_WINDOW_SECONDS );
  // Operation evaluators use the previous head's time, before the new block updates it.
  generate_at( post_time - fc::seconds( HIVE_BLOCK_INTERVAL ) );
  auto post_and_vote = [&]( const account_name_type& author, const fc::ecc::private_key& key )
  {
    comment_operation comment;
    comment.author = author;
    comment.permlink = "hf29-boundary";
    comment.parent_permlink = "test";
    comment.title = "HF29 cashout boundary";
    comment.body = "A real post with a real vote.";
    push_transaction( comment, key );

    vote_operation vote;
    vote.voter = author;
    vote.author = author;
    vote.permlink = comment.permlink;
    vote.weight = HIVE_100_PERCENT;
    push_transaction( vote, key );
    generate_block();

    const auto* cashout = db->find_comment_cashout( *db->get_comment( author, comment.permlink ) );
    BOOST_REQUIRE( cashout != nullptr );
    BOOST_REQUIRE_GT( cashout->get_net_rshares(), 0 );
    return cashout->get_cashout_time();
  };

  const auto before_time = post_and_vote( "alice", alice_post_key );
  const auto at_time = post_and_vote( "bob", bob_post_key );
  const auto after_time = post_and_vote( "carol", carol_post_key );
  BOOST_REQUIRE_EQUAL( before_time, activation_time() - fc::seconds( HIVE_BLOCK_INTERVAL ) );
  BOOST_REQUIRE_EQUAL( at_time, activation_time() );
  BOOST_REQUIRE_EQUAL( after_time, activation_time() + fc::seconds( HIVE_BLOCK_INTERVAL ) );

  db_plugin->debug_update( []( database& db )
  {
    db.modify( db.get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ),
      [&]( reward_fund_object& rf ) { rf.recent_claims = HIVE_HF21_CONVERGENT_LINEAR_RECENT_CLAIMS; } );
  } );
  generate_block();
  generate_at( before_time );
  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims > PIXA_HF29_RECENT_CLAIMS );
  BOOST_REQUIRE( db->find_comment_cashout( *db->get_comment( "alice", std::string( "hf29-boundary" ) ) ) == nullptr );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_rewards(), HBD_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hive_rewards(), HIVE_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_vest_rewards(), VEST_asset( 0 ) );

  generate_at( at_time );
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims == PIXA_HF29_RECENT_CLAIMS );
  BOOST_REQUIRE( db->find_comment_cashout( *db->get_comment( "bob", std::string( "hf29-boundary" ) ) ) == nullptr );
  // Cashouts precede process_hardforks within this same block: this post is still dust.
  BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_hbd_rewards(), HBD_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_hive_rewards(), HIVE_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_vest_rewards(), VEST_asset( 0 ) );

  generate_at( after_time );
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( db->find_comment_cashout( *db->get_comment( "carol", std::string( "hf29-boundary" ) ) ) == nullptr );
  BOOST_REQUIRE_GT( db->get_account( "carol" ).get_hbd_rewards(), HBD_asset( 0 ) );
  BOOST_REQUIRE_GT( db->get_account( "carol" ).get_vest_rewards(), VEST_asset( 0 ) );
  // Earlier cashouts are consumed, not retried or compensated after the reset.
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_vest_rewards(), VEST_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_vest_rewards(), VEST_asset( 0 ) );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
#endif
