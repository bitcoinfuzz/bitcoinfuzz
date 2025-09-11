// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_POLICY_FEES_H
#define BITCOIN_POLICY_FEES_H

#include <consensus/amount.h>
#include <policy/feerate.h>
#include <random.h>
#include <sync.h>
#include <threadsafety.h>
#include <uint256.h>
#include <util/fs.h>

#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>


// How often to flush fee estimates to fee_estimates.dat.
static constexpr std::chrono::hours FEE_FLUSH_INTERVAL{1};

/** fee_estimates.dat that are more than 60 hours (2.5 days) old will not be read,
 * as fee estimates are based on historical data and may be inaccurate if
 * network activity has changed.
 */
static constexpr std::chrono::hours MAX_FILE_AGE{60};

// Whether we allow importing a fee_estimates file older than MAX_FILE_AGE.
static constexpr bool DEFAULT_ACCEPT_STALE_FEE_ESTIMATES{false};

class AutoFile;
class TxConfirmStats;
struct RemovedMempoolTransactionInfo;
struct NewMempoolTransactionInfo;

/* Identifier for each of the 3 different TxConfirmStats which will track
 * history over different time horizons. */
enum class FeeEstimateHorizon {
    SHORT_HALFLIFE,
    MED_HALFLIFE,
    LONG_HALFLIFE,
};

static constexpr auto ALL_FEE_ESTIMATE_HORIZONS = std::array{
    FeeEstimateHorizon::SHORT_HALFLIFE,
    FeeEstimateHorizon::MED_HALFLIFE,
    FeeEstimateHorizon::LONG_HALFLIFE,
};

std::string StringForFeeEstimateHorizon(FeeEstimateHorizon horizon);

/* Enumeration of reason for returned fee estimate */
enum class FeeReason {
    NONE,
    HALF_ESTIMATE,
    FULL_ESTIMATE,
    DOUBLE_ESTIMATE,
    CONSERVATIVE,
    MEMPOOL_MIN,
    PAYTXFEE,
    FALLBACK,
    REQUIRED,
};

/* Used to return detailed information about a feerate bucket */
struct EstimatorBucket
{
    double start = -1;
    double end = -1;
    double withinTarget = 0;
    double totalConfirmed = 0;
    double inMempool = 0;
    double leftMempool = 0;
};

/* Used to return detailed information about a fee estimate calculation */
struct EstimationResult
{
    EstimatorBucket pass;
    EstimatorBucket fail;
    double decay = 0;
    unsigned int scale = 0;
};

struct FeeCalculation
{
    EstimationResult est;
    FeeReason reason = FeeReason::NONE;
    int desiredTarget = 0;
    int returnedTarget = 0;
};

class FeeFilterRounder
{
private:
    static constexpr double MAX_FILTER_FEERATE = 1e7;
    /** FEE_FILTER_SPACING is just used to provide some quantization of fee
     * filter results.  Historically it reused FEE_SPACING, but it is completely
     * unrelated, and was made a separate constant so the two concepts are not
     * tied together */
    static constexpr double FEE_FILTER_SPACING = 1.1;

public:
    /** Create new FeeFilterRounder */
    explicit FeeFilterRounder(const CFeeRate& min_incremental_fee, FastRandomContext& rng);

    /** Quantize a minimum fee for privacy purpose before broadcast. */
    CAmount round(CAmount currentMinFee) EXCLUSIVE_LOCKS_REQUIRED(!m_insecure_rand_mutex);

private:
    const std::set<double> m_fee_set;
    Mutex m_insecure_rand_mutex;
    FastRandomContext& insecure_rand GUARDED_BY(m_insecure_rand_mutex);
};

#endif // BITCOIN_POLICY_FEES_H
