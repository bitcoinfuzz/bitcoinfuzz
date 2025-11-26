#define template c_template  // avoid C++ keyword conflict just during includes

extern "C" {
    #include "common/bolt11.h"
    #include "common/bolt12.h"
    #include "bitcoin/pubkey.h"
    #include "common/node_id.h"
    #include "common/utils.h"
    #include "common/setup.h"
    #include "common/features.h"
    #include <common/decode_array.h>
    #include "common/addr.h"
    #include <common/ping.h>
    #include <common/sphinx.h>
    #include <common/derive_basepoints.h>
    #include <bitcoin/chainparams.h>
    #include <wire/peer_wiregen.h>
    #include <ccan/tal/tal.h>
}

#undef template

#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <memory>
#include <iostream>
#include <iostream>
#include <span>
#include "module.h"

struct CleanTmpCtxGuard {
    CleanTmpCtxGuard() noexcept = default;
    ~CleanTmpCtxGuard() noexcept { clean_tmpctx(); }
    CleanTmpCtxGuard(const CleanTmpCtxGuard&) = delete;
    CleanTmpCtxGuard& operator=(const CleanTmpCtxGuard&) = delete;
};

void init(int *argc, char ***argv) {
    if (!tmpctx){
        common_setup("fuzzer"); 
    }
}

std::string hex_encode(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

static bool ecdsa_sig_check_is_zero(const unsigned char sig64[64]) {
    static const unsigned char Z[32] = {0};
    const unsigned char* r = sig64;
    const unsigned char* s = sig64 + 32;

    if (memcmp(r, Z, 32) == 0) return true;          // r == 0
    if (memcmp(s, Z, 32) == 0) return true;          // s == 0
    return false;
}

std::optional<std::string> clightning_des_invoice(const std::string& input) {
    CleanTmpCtxGuard _cleanup;

    char* fail = nullptr;
    const struct chainparams* params = chainparams_for_network("bitcoin");

    struct bolt11 *invoice = bolt11_decode(tmpctx, input.c_str(), nullptr, nullptr, params, &fail);

    if (!invoice) {
        // Handle invoices without payment secrets by returning null
        // This is needed because LND don't require payment secrets,
        // and we need to maintain compatibility with that implementation
        if (strcmp(fail, "Missing required payment secret (s field)") == 0) {
            return std::nullopt;
        }
        return "";
    }

    std::ostringstream result;
    result << "HASH=" << hex_encode(invoice->payment_hash.u.u8, 32);

    result << ";PAYMENT_SECRET=";
    if (invoice->payment_secret) {
        result << hex_encode(invoice->payment_secret->data, 32);
    }

    result << ";AMOUNT=";
    if (invoice->msat) {
        result << invoice->msat->millisatoshis;
    } else {
        result << "0";
    }

    result << ";DESCRIPTION=";
    if (invoice->description) {
        result << invoice->description;
    }

    result << ";METADATA=";
    if (invoice->metadata) {
        result << hex_encode(invoice->metadata, tal_bytelen(invoice->metadata));
    }

    struct pubkey key;
    assert(pubkey_from_node_id(&key, &invoice->receiver_id));

    uint8_t compressed[33];
    pubkey_to_der(compressed, &key);
    result << ";RECIPIENT=" << hex_encode(compressed, 33);

    result << ";DESCRIPTION_HASH=";
    if (invoice->description_hash) {
        result << hex_encode(invoice->description_hash->u.u8, 32);
    }

    // Convert the expiry time to seconds, ensuring consistent overflow behavior
    // with other implementations like LND. The multiplication and division by 1000000000
    // may appear redundant, but it's intentionally mirroring the nanosecond conversion 
    // logic used in LND to ensure the same overflow characteristics.
    result << ";EXPIRY=" << (invoice->expiry * 1000000000) / 1000000000;

    result << ";TIMESTAMP=" << invoice->timestamp;

    result << ";FALLBACK_ADDRESS=";
    if (invoice->fallbacks) {
        // Use only the first fallback address for compatibility with LND,
        // which ignores additional fallback fields.
        // See: https://github.com/lightningnetwork/lnd/issues/9591
        std::string addr = encode_scriptpubkey_to_addr(tmpctx, params, invoice->fallbacks[0]);
        result << addr;
    }

    if (invoice->routes) {
        for (size_t i = 0; i < tal_count(invoice->routes); i++) {
            struct route_info *route = invoice->routes[i];
            result << ";PRIVATE_ROUTE=[";
            for (size_t j = 0; j < tal_count(route); j++) {
                if (j == 0) result << "(";
                else result << ",(";
                
                struct pubkey key;
                assert(pubkey_from_node_id(&key, &route[j].pubkey));
                uint8_t compressed[33];
                pubkey_to_der(compressed, &key);
                result << "NODE_ID=" << hex_encode(compressed, 33);
                result << ",SHORT_CHANNEL_ID=" << route[j].short_channel_id.u64;
                result << ",FEES=" << route[j].fee_base_msat;
                result << ",CLTV_EXPIRY_DELTA=" << route[j].cltv_expiry_delta;
                result << ",PROPORTIONAL_MILLIONTHS=" << route[j].fee_proportional_millionths;
                result << ")";
            }
            result << "]";
        }
    }

    result << ";MIN_CLTV=" << invoice->min_final_cltv_expiry;

    result << ";FEATURES=";
    if (invoice->features) {
        result << hex_encode(invoice->features, tal_bytelen(invoice->features));
    }

    return result.str();
}

std::string clightning_des_offer(const std::string_view input) {
    CleanTmpCtxGuard _cleanup;

    char* fail = nullptr;

    // Get the truncated length of the string (in case it contains null bytes)
    size_t c_string_len = strnlen(input.data(), input.size());

    struct tlv_offer *offer = offer_decode(tmpctx, input.data(), c_string_len, /*our_features=*/nullptr, /*must_be_chain=*/nullptr, &fail);
    if (!offer) {
        return "";
    }

    std::ostringstream result;
    result << "CHAINS=";
    if (offer->offer_chains && tal_count(offer->offer_chains) > 0) {
        for (size_t i = 0; i < tal_count(offer->offer_chains); i++) {
            if (i > 0) result << ";";
            result << hex_encode(offer->offer_chains[i].shad.sha.u.u8, 32);
        }
    } else {
        // If no chains are specified, we fallback to the bitcoin chain (this is
        // is necessary for compatibility with Eclair).
        struct bitcoin_blkid chain = chainparams_for_network("bitcoin")->genesis_blockhash;
        result << hex_encode(chain.shad.sha.u.u8, 32);
    }

    result << ";METADATA=";
    if (offer->offer_metadata) {
        result << hex_encode(offer->offer_metadata, tal_bytelen(offer->offer_metadata));
    }

    if (offer->offer_amount) {
        result << ";AMOUNT=";
        result << *offer->offer_amount;
    }

    if (offer->offer_currency) {
        result << ";CURRENCY=";
        size_t len = tal_bytelen(offer->offer_currency);
        result.write((const char*)offer->offer_currency, len);
    }

    result << ";DESCRIPTION=";
    if (offer->offer_description) {
        size_t len = tal_bytelen(offer->offer_description);
        result.write((const char*)offer->offer_description, len);
    }

    result << ";FEATURES=";
    if (offer->offer_features) {
        result << hex_encode(offer->offer_features, tal_bytelen(offer->offer_features));
    }

    result << ";ABSOLUTE_EXPIRY=";
    if (offer->offer_absolute_expiry) {
        result << *offer->offer_absolute_expiry;
    }

    if (offer->offer_paths) {
        for (size_t i = 0; i < tal_count(offer->offer_paths); i++) {
            struct blinded_path_hop **blinded_path_hops = offer->offer_paths[i]->path;

            for (size_t j = 0; j < tal_count(blinded_path_hops); j++) {
                result << ";PATH_" << i << "_HOP=";
                struct pubkey pubkey = blinded_path_hops[j]->blinded_node_id;
                uint8_t compressed[33];
                pubkey_to_der(compressed, &pubkey);
                result << hex_encode(compressed, 33);
            }
        }
    }

    result << ";ISSUER=";
    if (offer->offer_issuer) {
        size_t len = tal_bytelen(offer->offer_issuer);
        result.write((const char*)offer->offer_issuer, len);
    }

    result << ";QUANTITY=";
    if (offer->offer_quantity_max) {
        result << *offer->offer_quantity_max;
    }

    result << ";ISSUER_ID=";
    if (offer->offer_issuer_id) {
        uint8_t compressed[33];
        pubkey_to_der(compressed, offer->offer_issuer_id);
        result << hex_encode(compressed, 33);
    }
    
    return result.str();
}

std::optional<std::string> clightning_parse_p2p_lightning_message(std::span<const uint8_t> buffer) {
    CleanTmpCtxGuard _cleanup;

    u8 *msg = tal_arr(tmpctx, u8, buffer.size());
    memcpy(msg, buffer.data(), buffer.size());
    peer_wire msg_type = static_cast<enum peer_wire>(fromwire_peektype(msg));
    std::ostringstream result;

    if (msg_type == WIRE_WARNING) {
	    struct channel_id channel;
        u8 *data;

        if (!fromwire_warning(tmpctx, msg, &channel, &data)) {
            return "";
        }

        result << "MSG_TYPE=warning;CHANNEL_ID=" << hex_encode(channel.id, 32);
        result << ";DATA=" << tal_hex(tmpctx, data);
    } else if (msg_type == WIRE_INIT) {
        u8 *globalfeatures, *features;
        struct tlv_init_tlvs *tlvs;
        struct wireaddr *remote_addr = nullptr;

        if (!fromwire_init(tmpctx, msg, &globalfeatures, &features, &tlvs)) {
            return "";
        }

        u8 *features_combined = featurebits_or(tmpctx, globalfeatures, features);

        result << "MSG_TYPE=init";
        // Strip the leading zeros to maintain compatibility with LND
        // which strips leading zeros from the features type by default
        // similar to: https://github.com/bitcoinfuzz/bitcoinfuzz/issues/291
        size_t len = tal_bytelen(features_combined);
        result << ";FEATURES=";
        if (len > 0) {
            const u8* bytes = features_combined;
            size_t start = 0;
            
            // Remove leading zero bytes
            while (start < len - 1 && bytes[start] == 0) {
                start++;
            }

            if (bytes[start] != 0) {   
                // Convert from the first non-zero byte onwards
                const char* hex_str = tal_hexstr(tmpctx, bytes + start, len - start);
                result << hex_str;
            }
        }

        if (tlvs->networks) {
            result << ";NETWORKS=";
            for (size_t i = 0; i < tal_count(tlvs->networks); ++i) {
                result << hex_encode(tlvs->networks[i].shad.sha.u.u8, 32) << ",";
            }
        }

        if (tlvs->remote_addr) {
            const u8 *cursor = tlvs->remote_addr;
            size_t len = tal_bytelen(tlvs->remote_addr);

            remote_addr = tal(tmpctx, struct wireaddr);
            if (!fromwire_wireaddr(&cursor, &len, remote_addr)) {
                return "";
            }
            // C-lightning accepts extra trailing bytes in address parsing and
            // rust-lightning doesn't. So we skip.
            if (len != 0) {
                return std::nullopt;
            }

            // C-lightning doens't verify that the remote address is a valid DNS name with ASCII characters in decoding phase
            if (remote_addr->type == ADDR_TYPE_DNS) {
                return std::nullopt;
            }
            result << ";REMOTE_NETWORK_ADDRESS=";
            if (remote_addr->type == ADDR_TYPE_IPV6) {
                // Use hex representation to avoid differences between c-lightning and rust-lightning formats for IPV6
                result << hex_encode(remote_addr->addr, remote_addr->addrlen) << ":" << remote_addr->port;
            } else {
                result << fmt_wireaddr(tmpctx, remote_addr);
            }
        }
    } else if (msg_type == WIRE_ERROR) {
	    struct channel_id channel;
        u8 *data;

        if (!fromwire_error(tmpctx, msg, &channel, &data)) {
            return "";
        }

        result << "MSG_TYPE=error;CHANNEL_ID=" << hex_encode(channel.id, 32);
        result << ";DATA=" << tal_hex(tmpctx, data);
    } else if (msg_type == WIRE_PING) {
        u16 num_pong_bytes;
	    u8 *ignored;
        u8 *pong;

        if (!check_ping_make_pong(tmpctx, msg, &pong)) {
            return "";
        }
        if (!pong) {
            return "";
        }
        fromwire_ping(tmpctx, msg, &num_pong_bytes, &ignored);

        result << "MSG_TYPE=ping;NUM_PONG_BYTES=" << num_pong_bytes;
        result << ";IGNORED=" << tal_bytelen(ignored);
    } else if (msg_type == WIRE_PONG) {
	    u8 *ignored;

        if (!fromwire_pong(tmpctx, msg, &ignored)) {
            return "";
        }

        result << "MSG_TYPE=pong;IGNORED=" << tal_bytelen(ignored);
    } else if (msg_type == WIRE_FUNDING_CREATED) {
	    struct channel_id channel_id;
        struct bitcoin_txid txid;
        u16 funding_txout;
        struct bitcoin_signature sig;

        // Skip messages that are bigger than 132 bytes (the maximum size of a
        // funding created message). Since rust-lightning returns an error for
        // messages that are too big.
        if (tal_bytelen(msg) > 132) {
            return std::nullopt;
        }

        if (!fromwire_funding_created(msg, &channel_id,
				      &txid,
				      &funding_txout,
				      &sig.s)) {
            return "";
        }

        if (ecdsa_sig_check_is_zero(sig.s.data)) {
            return "";
        }

        result << "MSG_TYPE=funding_created;TEMPORARY_CHANNEL_ID=" << fmt_channel_id(tmpctx, &channel_id);
        result << ";FUNDING_TXID=" << fmt_bitcoin_txid(tmpctx, &txid);
        result << ";FUNDING_OUTPUT_INDEX=" << funding_txout;
        result << ";SIGNATURE=" << fmt_secp256k1_ecdsa_signature(tmpctx, &sig.s);
    } else if (msg_type == WIRE_OPEN_CHANNEL) {
        struct basepoints theirs;
        struct pubkey their_funding_pubkey, first_per_commitment_point;
        struct bitcoin_blkid chain_hash;
        struct channel_id channel_id;
        u8 channel_flags;
        u16 to_self_delay, max_accepted_htlcs;
        struct tlv_open_channel_tlvs *open_tlvs;
        struct amount_msat push_msat, max_htlc_value_in_flight, htlc_minimum;
        struct amount_sat funding_sats, dust_limit, channel_reserve;
        u32 feerate_per_kw;

        if (!fromwire_open_channel(tmpctx, msg, &chain_hash,
                    &channel_id,
                    &funding_sats,
                    &push_msat,
                    &dust_limit,
                    &max_htlc_value_in_flight,
                    &channel_reserve,
                    &htlc_minimum,
                    &feerate_per_kw,
                    &to_self_delay,
                    &max_accepted_htlcs,
                    &their_funding_pubkey,
                    &theirs.revocation,
                    &theirs.payment,
                    &theirs.delayed_payment,
                    &theirs.htlc,
                    &first_per_commitment_point,
                    &channel_flags,
                    &open_tlvs)) {
                        return "";
                    }

        result << "MSG_TYPE=open_channel";
        result << ";CHAIN_HASH=" << hex_encode(chain_hash.shad.sha.u.u8, 32);
        result << ";TEMPORARY_CHANNEL_ID=" << fmt_channel_id(tmpctx, &channel_id);
        result << ";FUNDING_SATOSHIS=" << funding_sats.satoshis;
        result << ";PUSH_MSAT=" << push_msat.millisatoshis;
        result << ";DUST_LIMIT_SATOSHIS=" << dust_limit.satoshis;
        result << ";MAX_HTLC_IN_FLIGHT_MSAT=" << max_htlc_value_in_flight.millisatoshis;
        result << ";CHANNEL_RESERVE_SATOSHIS=" << channel_reserve.satoshis;
        result << ";HTLC_MINIMUM_MSAT=" << htlc_minimum.millisatoshis;
        result << ";FEERATE_PER_KW=" << feerate_per_kw;
        result << ";TO_SELF_DELAY=" << to_self_delay;
        result << ";MAX_ACCEPTED_HTLCS=" << max_accepted_htlcs;
        result << ";FUNDING_PUBKEY=" << fmt_pubkey(tmpctx, &their_funding_pubkey);
        result << ";REVOCATION_BASEPOINT=" << fmt_pubkey(tmpctx, &theirs.revocation);
        result << ";PAYMENT_BASEPOINT=" << fmt_pubkey(tmpctx, &theirs.payment);
        result << ";DELAYED_PAYMENT_BASEPOINT=" << fmt_pubkey(tmpctx, &theirs.delayed_payment);
        result << ";HTLC_BASEPOINT=" << fmt_pubkey(tmpctx, &theirs.htlc);
        result << ";FIRST_PER_COMMITMENT_POINT=" << fmt_pubkey(tmpctx, &first_per_commitment_point);
        result << ";CHANNEL_FLAGS=" << std::hex << std::setw(1) << std::setfill('0')
            << static_cast<int>(channel_flags)
            << std::dec;

        if (open_tlvs && open_tlvs->upfront_shutdown_script) {
            result << ";UPFRONT_SHUTDOWN_SCRIPT=";
            result << tal_hex(tmpctx, open_tlvs->upfront_shutdown_script);
        }

        if (open_tlvs && open_tlvs->channel_type) {
            result << ";CHANNEL_TYPE=";
            // Strip the leading zeros to maintain compatibility with LND
            // which strips leading zeros from the channel type by default
            // more info: https://github.com/bitcoinfuzz/bitcoinfuzz/issues/291
            size_t len = tal_bytelen(open_tlvs->channel_type);

            if (len > 0) {
                const u8* bytes = open_tlvs->channel_type;
                size_t start = 0;
                
                // Remove leading zero bytes
                while (start < len - 1 && bytes[start] == 0) {
                    start++;
                }
                
                if (bytes[start] != 0) {   
                    // Convert from the first non-zero byte onwards
                    const char* hex_str = tal_hexstr(tmpctx, bytes + start, len - start);
                    result << hex_str;
                }
            }
        }
    } else if (msg_type == WIRE_FUNDING_SIGNED) {
        channel_id channel;
        secp256k1_ecdsa_signature signature;
        // Skip messages that are bigger than 98 bytes (the maximum size of a
        // funding signed message). Since rust-lightning returns an error for
        // messages that are too big.
        if (tal_bytelen(msg) > 98) {
            return std::nullopt;
        }

        if (!fromwire_funding_signed(msg, &channel, &signature)) {
            return "";
        }

        if (ecdsa_sig_check_is_zero(signature.data)) {
            return "";
        }

        result << "MSG_TYPE=funding_signed;CHANNEL_ID=" << fmt_channel_id(tmpctx, &channel);
        result << ";SIGNATURE=" << fmt_secp256k1_ecdsa_signature(tmpctx, &signature);
    } else if (msg_type == WIRE_CHANNEL_READY) {
        channel_id channel;
        pubkey second_per_commitment_point;
        tlv_channel_ready_tlvs *tlvs;

        if (!fromwire_channel_ready(tmpctx, msg, &channel, &second_per_commitment_point, &tlvs)) {
            return "";
        }

        result << "MSG_TYPE=channel_ready;CHANNEL_ID=" << fmt_channel_id(tmpctx, &channel);
        result << ";POINT=" << fmt_pubkey(tmpctx, &second_per_commitment_point);

        if (tlvs->short_channel_id) {
            result << ";ALIAS=" << tlvs->short_channel_id->u64;
        }
    } else if (msg_type == WIRE_SHUTDOWN) {
        channel_id channel;
        u8 *scriptpubkey;
        tlv_shutdown_tlvs *tlvs;

        if (!fromwire_shutdown(tmpctx, msg, &channel, &scriptpubkey, &tlvs)) {
            return "";
        }

        if (tlvs->fields) {
            for (size_t i = 0; i < tal_count(tlvs->fields); i++) {
                // In case we receive a wrong_funding TLV, we must skip it.
                // Since other implementations don't parse this even TLV.
                if (tlvs->fields[i].numtype == 100) {
                    return std::nullopt;
                }
            }
        }

        result << "MSG_TYPE=shutdown;CHANNEL_ID=" << fmt_channel_id(tmpctx, &channel);
        result << ";SCRIPTPUBKEY=" << tal_hex(tmpctx, scriptpubkey);
    } else if (msg_type == WIRE_CLOSING_SIGNED) {
        channel_id channel;
        secp256k1_ecdsa_signature signature;
        struct amount_sat fee_satoshis;
        tlv_closing_signed_tlvs *tlvs;

        if (!fromwire_closing_signed(tmpctx, msg, &channel, &fee_satoshis, &signature, &tlvs)) {
            return "";
        }

        if (ecdsa_sig_check_is_zero(signature.data)) {
            return "";
        }

        result << "MSG_TYPE=closing_signed;CHANNEL_ID=" << fmt_channel_id(tmpctx, &channel);
        result << ";FEE_SATOSHIS=" << fee_satoshis.satoshis; 
        result << ";SIGNATURE=" << fmt_secp256k1_ecdsa_signature(tmpctx, &signature);

        if (tlvs->fee_range) {
            result << ";FEE_RANGE_MIN=" << tlvs->fee_range->min_fee_satoshis.satoshis;
            result << ";FEE_RANGE_MAX=" << tlvs->fee_range->max_fee_satoshis.satoshis;
        }
    } else if (msg_type == WIRE_CLOSING_COMPLETE) {
        channel_id channel;
        u32 locktime;
        u8 *closer_scriptpubkey, *closee_scriptpubkey;
        struct amount_sat fee_satoshis;
        tlv_closing_tlvs *tlvs;

        if (!fromwire_closing_complete(tmpctx, msg, &channel, &closer_scriptpubkey, &closee_scriptpubkey, &fee_satoshis, &locktime, &tlvs)) {
            return "";
        }

        result << "MSG_TYPE=closing_complete;CHANNEL_ID=" << fmt_channel_id(tmpctx, &channel);
        result << ";CLOSER_SCRIPTPUBKEY=" << tal_hex(tmpctx, closer_scriptpubkey);
        result << ";CLOSEE_SCRIPTPUBKEY=" << tal_hex(tmpctx, closee_scriptpubkey);
        result << ";FEE_SATOSHIS=" << fee_satoshis.satoshis;
        result << ";LOCKTIME=" << locktime;

        if (tlvs->closer_and_closee_outputs) {
            if (ecdsa_sig_check_is_zero(tlvs->closer_and_closee_outputs->data)) {
                return "";
            }
            result << ";CLOSER_AND_CLOSEE_OUTPUTS_SIG=" << fmt_secp256k1_ecdsa_signature(tmpctx, tlvs->closer_and_closee_outputs);
        }

        if (tlvs->closer_output_only) {
            if (ecdsa_sig_check_is_zero(tlvs->closer_output_only->data)) {
                return "";
            }
            result << ";CLOSER_OUTPUT_SIG=" << fmt_secp256k1_ecdsa_signature(tmpctx, tlvs->closer_output_only);
        }

        if (tlvs->closee_output_only) {
            if (ecdsa_sig_check_is_zero(tlvs->closee_output_only->data)) {
                return "";
            }
            result << ";CLOSEE_OUTPUT_SIG=" << fmt_secp256k1_ecdsa_signature(tmpctx, tlvs->closee_output_only);
        }
    } else if (msg_type == WIRE_UPDATE_ADD_HTLC) {
        channel_id channel;
        u64 id;
        struct amount_msat amount;
        sha256 payment_hash;
        u32 cltv_expiry;
        u8 onion_routing_packet[TOTAL_PACKET_SIZE(ROUTING_INFO_SIZE)]; 
        struct tlv_update_add_htlc_tlvs *update_add_htlc;

        if (!fromwire_update_add_htlc(tmpctx, msg, &channel, &id, &amount, &payment_hash, &cltv_expiry, onion_routing_packet, &update_add_htlc)) {
            return "";
        }
        
        enum onion_wire failcode;
        onionpacket* onion;
        onion = parse_onionpacket(tmpctx, onion_routing_packet,
			       TOTAL_PACKET_SIZE(ROUTING_INFO_SIZE),
			       &failcode);
        
        if (!onion) {
            return "";
        }

        result << "MSG_TYPE=update_add_htlc;CHANNEL_ID=" << fmt_channel_id(tmpctx, &channel);
        result << ";ID=" << id;
        result << ";AMOUNT=" << amount.millisatoshis;
        result << ";PAYMENT_HASH=" << fmt_sha256(tmpctx, &payment_hash);
        result << ";EXPIRY=" << cltv_expiry;
        result << ";ONION_ROUTING_PACKET=[VERSION=" << (unsigned int)onion->version;
        result << ";PUBLIC_KEY=" << fmt_secp256k1_pubkey(tmpctx, &onion->ephemeralkey.pubkey);
        result << ";HOP_DATA=" << hex_encode(onion->routinginfo, ROUTING_INFO_SIZE);
        result << ";HMAC=" << hex_encode(onion->hmac.bytes, 32);
        result << "]";

        if (update_add_htlc->blinded_path) {
            result << ";BLINDED_PATH=" << fmt_pubkey(tmpctx, update_add_htlc->blinded_path);
        }
    }

    return result.str();
}

namespace bitcoinfuzz
{
    namespace module
    {
        CLightning::CLightning(void) : BaseModule("CLightning") {
            init(nullptr, nullptr);
        }

        std::optional<std::string> CLightning::deserialize_invoice(std::string str) const
        {
            return clightning_des_invoice(str.c_str());
        }

        std::optional<std::string> CLightning::deserialize_offer(std::string str) const
        {
            return clightning_des_offer(str);
        }

        std::optional<std::string> CLightning::parse_p2p_lightning_message(std::span<const uint8_t> buffer) const
        {
            return clightning_parse_p2p_lightning_message(buffer);
        }
    }
}
