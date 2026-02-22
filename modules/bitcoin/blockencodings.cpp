// Copyright (c) 2016-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockencodings.h>
#include <chainparams.h>
#include <common/system.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <crypto/siphash.h>
#include <random.h>
#include <streams.h>
#include <validation.h>

#include <unordered_map>

CBlockHeaderAndShortTxIDs::CBlockHeaderAndShortTxIDs(const CBlock& block, uint64_t nonce)
    : nonce(nonce),
      shorttxids(block.vtx.size() - 1),
      prefilledtxn(1),
      header(block)
{
    FillShortTxIDSelector();
    // TODO: Use our mempool prior to block acceptance to predictively fill more than just the coinbase
    prefilledtxn[0] = {0, block.vtx[0]};
    for (size_t i = 1; i < block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];
        shorttxids[i - 1] = GetShortID(tx.GetWitnessHash());
    }
}

void CBlockHeaderAndShortTxIDs::FillShortTxIDSelector() const
{
    DataStream stream{};
    stream << header << nonce;
    CSHA256 hasher;
    hasher.Write((unsigned char*)&(*stream.begin()), stream.end() - stream.begin());
    uint256 shorttxidhash;
    hasher.Finalize(shorttxidhash.begin());
    m_hasher.emplace(shorttxidhash.GetUint64(0), shorttxidhash.GetUint64(1));
}

uint64_t CBlockHeaderAndShortTxIDs::GetShortID(const Wtxid& wtxid) const
{
    static_assert(SHORTTXIDS_LENGTH == 6, "shorttxids calculation assumes 6-byte shorttxids");
    return (*Assert(m_hasher))(wtxid.ToUint256()) & 0xffffffffffffL;
}

bool PartiallyDownloadedBlock::IsTxAvailable(size_t index) const
{
    if (header.IsNull()) return false;

    assert(index < txn_available.size());
    return txn_available[index] != nullptr;
}

ReadStatus PartiallyDownloadedBlock::FillBlock(CBlock& block, const std::vector<CTransactionRef>& vtx_missing, bool segwit_active)
{
    if (header.IsNull()) return READ_STATUS_INVALID;

    block = header;
    block.vtx.resize(txn_available.size());

    size_t tx_missing_offset = 0;
    for (size_t i = 0; i < txn_available.size(); i++) {
        if (!txn_available[i]) {
            if (tx_missing_offset >= vtx_missing.size()) {
                return READ_STATUS_INVALID;
            }
            block.vtx[i] = vtx_missing[tx_missing_offset++];
        } else {
            block.vtx[i] = std::move(txn_available[i]);
        }
    }

    // Make sure we can't call FillBlock again.
    header.SetNull();
    txn_available.clear();

    if (vtx_missing.size() != tx_missing_offset) {
        return READ_STATUS_INVALID;
    }

    // Check for possible mutations early now that we have a seemingly good block
    IsBlockMutatedFn check_mutated{m_check_block_mutated_mock ? m_check_block_mutated_mock : IsBlockMutated};
    if (check_mutated(/*block=*/block, /*check_witness_root=*/segwit_active)) {
        return READ_STATUS_FAILED; // Possible Short ID collision
    }

    return READ_STATUS_OK;
}
