// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#include <arith_uint256.h>
#include <attributes.h>
#include <script/script_error.h>
#include <script/sigcache.h>
#include <primitives/block.h>
#include <uint256.h>
#include <tinyformat.h>

#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/** Check if a block has been mutated (with respect to its merkle root and witness commitments). */
bool IsBlockMutated(const CBlock& block, bool check_witness_root);

#endif // BITCOIN_VALIDATION_H
