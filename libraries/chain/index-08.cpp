#include <hive/chain/hive_objects.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_08( database& db )
{
  HIVE_ADD_CORE_INDEX(db, escrow_index);
  HIVE_ADD_CORE_INDEX(db, savings_withdraw_index);
  HIVE_ADD_CORE_INDEX(db, decline_voting_rights_request_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::escrow_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::savings_withdraw_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::decline_voting_rights_request_index)
// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::escrow_index>& chainbase::database::get_index<hive::chain::escrow_index>() const;
template chainbase::generic_index<hive::chain::escrow_index>& chainbase::database::get_mutable_index<hive::chain::escrow_index>();

template const chainbase::generic_index<hive::chain::savings_withdraw_index>& chainbase::database::get_index<hive::chain::savings_withdraw_index>() const;
template chainbase::generic_index<hive::chain::savings_withdraw_index>& chainbase::database::get_mutable_index<hive::chain::savings_withdraw_index>();

template const chainbase::generic_index<hive::chain::decline_voting_rights_request_index>& chainbase::database::get_index<hive::chain::decline_voting_rights_request_index>() const;
template chainbase::generic_index<hive::chain::decline_voting_rights_request_index>& chainbase::database::get_mutable_index<hive::chain::decline_voting_rights_request_index>();

