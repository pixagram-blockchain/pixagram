#include <hive/chain/comment_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/witness_objects.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_04( database& db )
{
  HIVE_ADD_CORE_INDEX(db, comment_vote_index);
  HIVE_ADD_CORE_INDEX(db, witness_vote_index);
  HIVE_ADD_CORE_INDEX(db, limit_order_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_vote_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_vote_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::limit_order_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::comment_vote_index>& chainbase::database::get_index<hive::chain::comment_vote_index>() const;
template chainbase::generic_index<hive::chain::comment_vote_index>& chainbase::database::get_mutable_index<hive::chain::comment_vote_index>();

template const chainbase::generic_index<hive::chain::witness_vote_index>& chainbase::database::get_index<hive::chain::witness_vote_index>() const;
template chainbase::generic_index<hive::chain::witness_vote_index>& chainbase::database::get_mutable_index<hive::chain::witness_vote_index>();

template const chainbase::generic_index<hive::chain::limit_order_index>& chainbase::database::get_index<hive::chain::limit_order_index>() const;
template chainbase::generic_index<hive::chain::limit_order_index>& chainbase::database::get_mutable_index<hive::chain::limit_order_index>();

