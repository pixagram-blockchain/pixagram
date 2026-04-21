#include <hive/chain/account_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/transaction_object.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_02( database& db )
{
  HIVE_ADD_CORE_INDEX(db, account_authority_index);
  HIVE_ADD_CORE_INDEX(db, witness_index);
  HIVE_ADD_CORE_INDEX(db, transaction_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_authority_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::transaction_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::account_authority_index>& chainbase::database::get_index<hive::chain::account_authority_index>() const;
template chainbase::generic_index<hive::chain::account_authority_index>& chainbase::database::get_mutable_index<hive::chain::account_authority_index>();

template const chainbase::generic_index<hive::chain::witness_index>& chainbase::database::get_index<hive::chain::witness_index>() const;
template chainbase::generic_index<hive::chain::witness_index>& chainbase::database::get_mutable_index<hive::chain::witness_index>();

template const chainbase::generic_index<hive::chain::transaction_index>& chainbase::database::get_index<hive::chain::transaction_index>() const;
template chainbase::generic_index<hive::chain::transaction_index>& chainbase::database::get_mutable_index<hive::chain::transaction_index>();

