#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/witness_objects.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_03( database& db )
{
  HIVE_ADD_CORE_INDEX(db, block_summary_index);
  HIVE_ADD_CORE_INDEX(db, witness_schedule_index);
  HIVE_ADD_CORE_INDEX(db, comment_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::block_summary_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_schedule_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_index)
// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::block_summary_index>& chainbase::database::get_index<hive::chain::block_summary_index>() const;
template chainbase::generic_index<hive::chain::block_summary_index>& chainbase::database::get_mutable_index<hive::chain::block_summary_index>();

template const chainbase::generic_index<hive::chain::witness_schedule_index>& chainbase::database::get_index<hive::chain::witness_schedule_index>() const;
template chainbase::generic_index<hive::chain::witness_schedule_index>& chainbase::database::get_mutable_index<hive::chain::witness_schedule_index>();

template const chainbase::generic_index<hive::chain::comment_index>& chainbase::database::get_index<hive::chain::comment_index>() const;
template chainbase::generic_index<hive::chain::comment_index>& chainbase::database::get_mutable_index<hive::chain::comment_index>();

