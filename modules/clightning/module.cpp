#define template c_template  // avoid C++ keyword conflict just during includes

extern "C" {
    #include "common/bolt11.h"
    #include "common/bolt12.h"
    #include "bitcoin/pubkey.h"
    #include "common/node_id.h"
    #include "common/utils.h"
    #include "common/setup.h"
    #include <bitcoin/chainparams.h>
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

std::string clightning_des_invoice(const std::string& input) {
    char* fail = nullptr;
    const struct chainparams* params = chainparams_for_network("bitcoin");

    struct bolt11 *invoice = bolt11_decode(tmpctx, input.c_str(), nullptr, nullptr, params, &fail);

    if (!invoice) {
        clean_tmpctx();
        return "";
    }

    std::ostringstream result;
    result << "HASH=" << hex_encode(invoice->payment_hash.u.u8, 32);

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

    struct pubkey key;
    assert(pubkey_from_node_id(&key, &invoice->receiver_id));

    uint8_t compressed[33];
    pubkey_to_der(compressed, &key);
    result << ";RECIPIENT=" << hex_encode(compressed, 33);

    // Convert the expiry time to seconds, ensuring consistent overflow behavior
    // with other implementations like LND. The multiplication and division by 1000000000
    // may appear redundant, but it's intentionally mirroring the nanosecond conversion 
    // logic used in LND to ensure the same overflow characteristics.
    result << ";EXPIRY=" << (invoice->expiry * 1000000000) / 1000000000;
    result << ";TIMESTAMP=" << invoice->timestamp;
    result << ";ROUTING_HINTS=" << tal_count(invoice->routes);
    result << ";MIN_CLTV=" << invoice->min_final_cltv_expiry;

    clean_tmpctx();
    return result.str();
}

std::string clightning_des_offer(const std::string_view input) {
    char* fail = nullptr;
    const struct chainparams* params = chainparams_for_network("bitcoin");

    // Get the truncated length of the string (in case it contains null bytes)
    size_t c_string_len = strnlen(input.data(), input.size());

    struct tlv_offer *offer = offer_decode(tmpctx, input.data(), c_string_len, nullptr, params, &fail);
    if (!offer) {
        clean_tmpctx();
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
        // If no chains are specified, Clightning defaults to bitcoin
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
        for (size_t i = 0; offer->offer_paths[i] != NULL; i++) {
            struct blinded_path_hop **blinded_path_hops = offer->offer_paths[i]->path;

            for (size_t j = 0; blinded_path_hops[j] != NULL; j++) {
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
    
    clean_tmpctx();
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
    }
}
