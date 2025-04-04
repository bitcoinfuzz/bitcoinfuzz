#include "clightning_lib.h"
#include "common/bolt11.h"
#include "bitcoin/pubkey.h"
#include "common/node_id.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <bitcoin/chainparams.h>
#include <inttypes.h>

static char* hex_encode(const unsigned char* data, size_t len) {
    char* hex = malloc(len * 2 + 1);
    if (!hex) return strdup("");
    for (size_t i = 0; i < len; ++i) {
        sprintf(hex + i * 2, "%02x", data[i]);
    }
    hex[len * 2] = '\0';
    return hex;
}

char* clightning_des_invoice(const char* input) {
    char *fail = NULL;
    struct bolt11 *invoice = NULL;
    struct pubkey key;
    const struct chainparams *params = chainparams_for_network("bitcoin");

    invoice = bolt11_decode(NULL, input, NULL, NULL, params, &fail);
    if (!invoice) {
        if (fail) {
            tal_free(fail);
        }
        return strdup("");
    }

    char *result = malloc(1024);
    if (!result) {
        tal_free(invoice);
        return strdup("");
    }
    result[0] = '\0';

    char* hash_str = hex_encode(invoice->payment_hash.u.u8, 32);
    strcat(result, "HASH=");
    if (hash_str) {
        strcat(result, hash_str);
        free(hash_str);
    }
    strcat(result, ";");

    strcat(result, "AMOUNT=");
    if (invoice->msat) {
        char amount_str[32];
        snprintf(amount_str, sizeof(amount_str), "%" PRIu64, invoice->msat->millisatoshis);
        strcat(result, amount_str);
    } else {
        strcat(result, "0");
    }
    strcat(result, ";");

    strcat(result, "DESCRIPTION=");
    if (invoice->description) {
        strcat(result, invoice->description);
    }
    strcat(result, ";");

    if (!pubkey_from_node_id(&key, &invoice->receiver_id)) {
        tal_free(invoice);
        free(result);
        return strdup("");
    }

    u8 compressed[33];
    pubkey_to_der(compressed, &key);
    char* recipient_str = hex_encode(compressed, 33);
    strcat(result, "RECIPIENT=");
    if (recipient_str) {
        strcat(result, recipient_str);
        free(recipient_str);
    }
    strcat(result, ";");

    char expiry_str[32];
    snprintf(expiry_str, sizeof(expiry_str), "%" PRIu64, invoice->expiry);
    strcat(result, "EXPIRY=");
    strcat(result, expiry_str);
    strcat(result, ";");

    char timestamp_str[32];
    snprintf(timestamp_str, sizeof(timestamp_str), "%" PRIu64, invoice->timestamp);
    strcat(result, "TIMESTAMP=");
    strcat(result, timestamp_str);
    strcat(result, ";");

    char routing_str[32];
    snprintf(routing_str, sizeof(routing_str), "%zu", tal_count(invoice->routes));
    strcat(result, "ROUTING_HINTS=");
    strcat(result, routing_str);
    strcat(result, ";");

    char cltv_str[16];
    snprintf(cltv_str, sizeof(cltv_str), "%u", invoice->min_final_cltv_expiry);
    strcat(result, "MIN_CLTV=");
    strcat(result, cltv_str);

    tal_free(invoice);
    return result;
}
