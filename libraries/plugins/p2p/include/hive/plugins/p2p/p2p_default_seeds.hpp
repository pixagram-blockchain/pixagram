#pragma once

#include <vector>

namespace hive{ namespace plugins { namespace p2p {

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_BUILD)
const std::vector< std::string > default_seeds;
#else
const std::vector< std::string > default_seeds = {
  "api.pixagram.com:2001"
};
#endif

} } } // hive::plugins::p2p
