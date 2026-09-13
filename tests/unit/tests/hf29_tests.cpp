#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/database.hpp>
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
  hf29_database_fixture()
    : hardfork_database_fixture( database_fixture::shared_file_size_small, HIVE_HARDFORK_1_28 )
  {}

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

  db->set_hardfork( HIVE_HARDFORK_1_29 );

  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims == PIXA_HF29_RECENT_CLAIMS );

  const auto& hfp = db->get_hardfork_property_object();
  BOOST_REQUIRE( hfp.current_hardfork_version == HIVE_HARDFORK_1_29_VERSION );
  BOOST_REQUIRE_EQUAL( hfp.last_hardfork, uint32_t( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE_EQUAL( hfp.processed_hardforks.size(), size_t( HIVE_HARDFORK_1_29 + 1 ) );

  generate_block();
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( recent_claims_reset_never_raises )
{ try {
  BOOST_TEST_MESSAGE( "Testing: HF29 leaves a fund that is already below the target alone" );

  const fc::uint128_t below_target = PIXA_HF29_RECENT_CLAIMS / 2;
  set_recent_claims( below_target );

  db->set_hardfork( HIVE_HARDFORK_1_29 );

  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29 ) );
  BOOST_REQUIRE( post_reward_fund().recent_claims == below_target );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( recent_claims_reset_leaves_empty_fund_alone )
{ try {
  BOOST_TEST_MESSAGE( "Testing: HF29 does not seed an empty fund" );

  set_recent_claims( 0 );

  db->set_hardfork( HIVE_HARDFORK_1_29 );

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
#endif
