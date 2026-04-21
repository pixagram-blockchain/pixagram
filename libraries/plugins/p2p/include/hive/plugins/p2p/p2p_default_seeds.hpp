#pragma once

#include <vector>

namespace hive{ namespace plugins { namespace p2p {

// Pre-mainnet: no default seeds baked into the binary. Operators must supply
// seeds via --p2p-seed-node or config.ini. Populate this list once real
// Pixagram seed nodes are announced.
const std::vector< std::string > default_seeds;

} } } // hive::plugins::p2p
