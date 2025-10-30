use lightning::bitcoin::constants::ChainHash;
use lightning::bitcoin::hex::{Case, DisplayHex};
use lightning::bitcoin::secp256k1::ecdsa::Signature;
use lightning::bolt11_invoice::{
    Bolt11Invoice, Bolt11InvoiceDescriptionRef, Bolt11SemanticError, Currency, ParseOrSemanticError,
};
use lightning::ln::msgs::{self, DecodeError};
use lightning::offers::offer::{self, Offer};
use lightning::util::ser::LengthReadable;
use std::ffi::CString;
use std::os::raw::c_char;
use std::{ffi::CStr, str::FromStr};

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

#[no_mangle]
pub unsafe extern "C" fn ldk_des_invoice(input: *const std::os::raw::c_char) -> *mut c_char {
    if input.is_null() {
        return str_to_c_string("");
    }

    // Convert C string to Rust string
    let c_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return str_to_c_string(""),
    };

    match Bolt11Invoice::from_str(c_str) {
        Ok(invoice) => {
            if invoice.currency() != Currency::Bitcoin {
                return str_to_c_string("");
            }
            let mut result = String::new();

            result.push_str("HASH=");
            result.push_str(&invoice.payment_hash().to_string());

            result.push_str(";PAYMENT_SECRET=");
            result.push_str(&invoice.payment_secret().to_string());

            result.push_str(";AMOUNT=");
            if let Some(amount) = invoice.amount_milli_satoshis() {
                result.push_str(&amount.to_string());
            } else {
                result.push_str("0");
            }

            result.push_str(";DESCRIPTION=");
            if let Bolt11InvoiceDescriptionRef::Direct(direct_description) = invoice.description() {
                result.push_str(&direct_description.to_string());
            }

            result.push_str(";METADATA=");
            if let Some(metadata) = invoice.payment_metadata() {
                result.push_str(&metadata.to_hex_string(Case::Lower));
            }

            let invoice_payee_pub_key = match invoice.payee_pub_key() {
                Some(payee) => payee.clone(),
                None => invoice.recover_payee_pub_key(),
            };

            result.push_str(";RECIPIENT=");
            result.push_str(&invoice_payee_pub_key.to_string());

            result.push_str(";DESCRIPTION_HASH=");
            if let Bolt11InvoiceDescriptionRef::Hash(hash_description) = invoice.description() {
                result.push_str(&hash_description.0.to_string());
            }

            // Convert the expiry time to seconds, ensuring consistent overflow behavior
            // with other implementations like LND. The multiplication and division by 1000000000
            // may appear redundant, but it's intentionally mirroring the nanosecond conversion
            // logic used in LND to ensure the same overflow characteristics.
            result.push_str(";EXPIRY=");
            result.push_str(
                ((invoice.expiry_time().as_secs() * 1000000000) / 1000000000)
                    .to_string()
                    .as_str(),
            );

            result.push_str(";TIMESTAMP=");
            result.push_str(
                &invoice
                    .clone()
                    .into_signed_raw()
                    .raw_invoice()
                    .data
                    .timestamp
                    .as_unix_timestamp()
                    .to_string(),
            );

            result.push_str(";FALLBACK_ADDRESS=");
            // Use only the first fallback address for compatibility with LND,
            // which ignores additional fallback fields.
            // See: https://github.com/lightningnetwork/lnd/issues/9591
            let fallback = invoice.fallback_addresses().first().cloned();
            if let Some(addr) = fallback {
                result.push_str(&addr.to_string());
            }

            invoice.private_routes().iter().for_each(|route| {
                result.push_str(&";PRIVATE_ROUTE=[");
                route.0.iter().enumerate().for_each(|(i, hop)| {
                    if i == 0 {
                        result.push_str("(");
                    } else {
                        result.push_str(",(");
                    }
                    result.push_str("NODE_ID=");
                    result.push_str(&hop.src_node_id.to_string());
                    result.push_str(",SHORT_CHANNEL_ID=");
                    result.push_str(&hop.short_channel_id.to_string());
                    result.push_str(",FEES=");
                    result.push_str(&hop.fees.base_msat.to_string());
                    result.push_str(",CLTV_EXPIRY_DELTA=");
                    result.push_str(&hop.cltv_expiry_delta.to_string());
                    result.push_str(",PROPORTIONAL_MILLIONTHS=");
                    result.push_str(&hop.fees.proportional_millionths.to_string());
                    result.push_str(")");
                });
                result.push_str("]");
            });

            result.push_str(";MIN_CLTV=");
            result.push_str(&invoice.min_final_cltv_expiry_delta().to_string());

            result.push_str(";FEATURES=");
            if invoice.features().is_some() {
                let mut flags = invoice.features().unwrap().le_flags().to_vec();
                flags.reverse();
                result.push_str(&flags.to_hex_string(Case::Lower));
            }

            str_to_c_string(&result)
        }
        // Handle invoices without payment secrets by returning null
        // This is needed because some Lightning implementations don't require payment secrets,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::NoPaymentSecret)) => {
            std::ptr::null_mut()
        }
        // Handle invoices with multiple payment hashes by returning null
        // This is needed because some Lightning implementations don't require payment to have only one hash,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::MultiplePaymentHashes)) => {
            std::ptr::null_mut()
        }
        // Handle invoices with multiple descriptions hashes by returning null
        // This is needed because some Lightning implementations don't require to have only one description,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::MultipleDescriptions)) => {
            std::ptr::null_mut()
        }
        Err(_) => str_to_c_string(""),
    }
}

#[no_mangle]
pub unsafe extern "C" fn ldk_des_offer(input: *const std::os::raw::c_char) -> *mut c_char {
    if input.is_null() {
        return str_to_c_string("");
    }

    // Convert C string to Rust string
    let c_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return str_to_c_string(""),
    };

    match Offer::from_str(c_str) {
        Ok(offer) => {
            let mut result = String::new();

            result.push_str("CHAINS=");
            // If no chains are specified, we fallback to the bitcoin chain (this is
            // is necessary for compatibility with Eclair).
            if offer.chains().is_empty() {
                result.push_str(&format!("{}", ChainHash::BITCOIN));
            } else {
                offer.chains().iter().enumerate().for_each(|(i, chain)| {
                    if i > 0 {
                        result.push_str(";");
                    }
                    result.push_str(&format!("{}", chain.to_string()));
                });
            }

            result.push_str(";METADATA=");
            if let Some(metadata) = offer.metadata() {
                result.push_str(&metadata.to_hex_string(Case::Lower));
            }

            if let Some(amount) = offer.amount() {
                match amount {
                    offer::Amount::Bitcoin { amount_msats } => {
                        result.push_str(";AMOUNT=");
                        result.push_str(&amount_msats.to_string());
                    }
                    offer::Amount::Currency {
                        iso4217_code,
                        amount,
                    } => {
                        result.push_str(";AMOUNT=");
                        result.push_str(&amount.to_string());
                        result.push_str(";CURRENCY=");
                        result.push_str(iso4217_code.as_str());
                    }
                }
            }

            result.push_str(";DESCRIPTION=");
            if let Some(description) = offer.description() {
                result.push_str(description.0);
            }

            result.push_str(";FEATURES=");
            let features = offer.offer_features();
            let mut be_flags = features.le_flags().to_vec();
            be_flags.reverse();
            result.push_str(be_flags.to_hex_string(Case::Lower).as_str());

            result.push_str(";ABSOLUTE_EXPIRY=");
            if let Some(absolute_expiry) = offer.absolute_expiry() {
                result.push_str(absolute_expiry.as_secs().to_string().as_str());
            }

            offer.paths().iter().enumerate().for_each(|(i, path)| {
                path.blinded_hops().iter().for_each(|hop| {
                    result.push_str(&format!(";PATH_{}_HOP=", i));
                    result.push_str(hop.blinded_node_id.to_string().as_str());
                });
            });

            result.push_str(";ISSUER=");
            if let Some(issuer) = offer.issuer() {
                result.push_str(issuer.0);
            }

            result.push_str(";QUANTITY=");
            let quantity = offer.supported_quantity();
            match quantity {
                offer::Quantity::Bounded(n) => result.push_str(&n.to_string()),
                _ => (),
            }

            result.push_str(";ISSUER_ID=");
            if let Some(issuer_id) = offer.issuer_signing_pubkey() {
                result.push_str(&issuer_id.to_string());
            }

            str_to_c_string(&result)
        }
        Err(_) => str_to_c_string(""),
    }
}

#[no_mangle]
pub unsafe extern "C" fn ldk_parse_p2p_lightning_message(
    data: *const u8,
    len: usize,
) -> *mut c_char {
    let data = std::slice::from_raw_parts(data, len);
    if data.len() < 2 {
        return str_to_c_string("");
    }

    let msg_type = u16::from_be_bytes([data[0], data[1]]);
    let mut payload = &data[2..];

    match msg_type {
        1 => match msgs::WarningMessage::read_from_fixed_length_buffer(&mut payload) {
            Ok(warning) => str_to_c_string(
                format!(
                    "MSG_TYPE=warning;CHANNEL_ID={};DATA={}",
                    warning.channel_id.0.to_hex_string(Case::Lower),
                    warning.data.as_bytes().to_hex_string(Case::Lower)
                )
                .as_str(),
            ),
            // Rust-lightning try to parse the error data as a UTF-8 string.
            // However, other implementations like LND and C-lightning, do not do this.
            Err(DecodeError::InvalidValue) => std::ptr::null_mut(),
            Err(_) => str_to_c_string(""),
        },
        17 => match msgs::ErrorMessage::read_from_fixed_length_buffer(&mut payload) {
            Ok(error) => str_to_c_string(
                format!(
                    "MSG_TYPE=error;CHANNEL_ID={};DATA={}",
                    error.channel_id.0.to_hex_string(Case::Lower),
                    error.data.as_bytes().to_hex_string(Case::Lower)
                )
                .as_str(),
            ),
            // Rust-lightning try to parse the error data as a UTF-8 string.
            // However, other implementations like LND and C-lightning, do not do this.
            Err(DecodeError::InvalidValue) => std::ptr::null_mut(),
            Err(_) => str_to_c_string(""),
        },
        18 => match msgs::Ping::read_from_fixed_length_buffer(&mut payload) {
            Ok(ping) => {
                if ping.ponglen >= 65532 {
                    str_to_c_string("")
                } else {
                    str_to_c_string(
                        format!(
                            "MSG_TYPE=ping;NUM_PONG_BYTES={};IGNORED={}",
                            ping.ponglen, ping.byteslen
                        )
                        .as_str(),
                    )
                }
            }
            Err(_) => str_to_c_string(""),
        },
        19 => match msgs::Pong::read_from_fixed_length_buffer(&mut payload) {
            Ok(pong) => {
                str_to_c_string(format!("MSG_TYPE=pong;IGNORED={}", pong.byteslen).as_str())
            }
            Err(_) => str_to_c_string(""),
        },
        32 => match msgs::OpenChannel::read_from_fixed_length_buffer(&mut payload) {
            Ok(open_channel) => {
                let common = open_channel.common_fields;
                let mut s = format!(
                    concat!(
                        "MSG_TYPE=open_channel",
                        ";CHAIN_HASH={}",
                        ";TEMPORARY_CHANNEL_ID={}",
                        ";FUNDING_SATOSHIS={}",
                        ";PUSH_MSAT={}",
                        ";DUST_LIMIT_SATOSHIS={}",
                        ";MAX_HTLC_IN_FLIGHT_MSAT={}",
                        ";CHANNEL_RESERVE_SATOSHIS={}",
                        ";HTLC_MINIMUM_MSAT={}",
                        ";FEERATE_PER_KW={}",
                        ";TO_SELF_DELAY={}",
                        ";MAX_ACCEPTED_HTLCS={}",
                        ";FUNDING_PUBKEY={}",
                        ";REVOCATION_BASEPOINT={}",
                        ";PAYMENT_BASEPOINT={}",
                        ";DELAYED_PAYMENT_BASEPOINT={}",
                        ";HTLC_BASEPOINT={}",
                        ";FIRST_PER_COMMITMENT_POINT={}",
                        ";CHANNEL_FLAGS={:x}",
                    ),
                    common.chain_hash.to_string(),
                    common.temporary_channel_id.to_string(),
                    common.funding_satoshis,
                    open_channel.push_msat,
                    common.dust_limit_satoshis,
                    common.max_htlc_value_in_flight_msat,
                    open_channel.channel_reserve_satoshis,
                    common.htlc_minimum_msat,
                    common.commitment_feerate_sat_per_1000_weight,
                    common.to_self_delay,
                    common.max_accepted_htlcs,
                    common.funding_pubkey.to_string(),
                    common.revocation_basepoint.to_string(),
                    common.payment_basepoint.to_string(),
                    common.delayed_payment_basepoint.to_string(),
                    common.htlc_basepoint.to_string(),
                    common.first_per_commitment_point.to_string(),
                    common.channel_flags,
                );
                if let Some(script) = &common.shutdown_scriptpubkey {
                    s.push_str(";UPFRONT_SHUTDOWN_SCRIPT=");
                    s.push_str(&script.as_bytes().to_lower_hex_string());
                }
                if let Some(channel_type) = &common.channel_type {
                    s.push_str(";CHANNEL_TYPE=");
                    let mut flags = channel_type.le_flags().to_vec();
                    flags.reverse();

                    // Strip the leading zeros to maintain compatibility with LND
                    // which strips leading zeros from the channel type by default
                    // more info: https://github.com/bitcoinfuzz/bitcoinfuzz/issues/291
                    while flags.first() == Some(&0) && flags.len() > 1 {
                        flags.remove(0);
                    }

                    let hex_str = flags.to_lower_hex_string();
                    s.push_str(&hex_str);
                }
                str_to_c_string(&s)
            }
            Err(_) => str_to_c_string(""),
        },
        34 => match msgs::FundingCreated::read_from_fixed_length_buffer(&mut payload) {
            Ok(funding_created) => {
                if sig_check_is_zero(&funding_created.signature) {
                    return str_to_c_string("");
                }
                str_to_c_string(&format!("MSG_TYPE=funding_created;TEMPORARY_CHANNEL_ID={};FUNDING_TXID={};FUNDING_OUTPUT_INDEX={};SIGNATURE={}", 
                funding_created.temporary_channel_id,
                funding_created.funding_txid.to_string(),
                funding_created.funding_output_index,
                funding_created.signature.to_string()
            ))
            }
            Err(DecodeError::UnknownRequiredFeature) => std::ptr::null_mut(),
            Err(_) => str_to_c_string(""),
        },
        35 => match msgs::FundingSigned::read_from_fixed_length_buffer(&mut payload) {
            Ok(funding_signed) => {
                if sig_check_is_zero(&funding_signed.signature) {
                    return str_to_c_string("");
                }
                str_to_c_string(&format!(
                    "MSG_TYPE=funding_signed;CHANNEL_ID={};SIGNATURE={}",
                    funding_signed.channel_id.to_string(),
                    funding_signed.signature.to_string()
                ))
            }
            Err(_) => str_to_c_string(""),
        },
        36 => match msgs::ChannelReady::read_from_fixed_length_buffer(&mut payload) {
            Ok(channel_ready) => {
                let mut result = format!(
                    "MSG_TYPE=channel_ready;CHANNEL_ID={};POINT={}",
                    channel_ready.channel_id.to_string(),
                    channel_ready.next_per_commitment_point.to_string(),
                );
                if let Some(alias) = channel_ready.short_channel_id_alias {
                    result.push_str(&format!(";ALIAS={}", alias.to_string()));
                }
                str_to_c_string(&result)
            }
            Err(_) => str_to_c_string(""),
        },
        38 => match msgs::Shutdown::read_from_fixed_length_buffer(&mut payload) {
            Ok(shutdown) => str_to_c_string(&format!(
                "MSG_TYPE=shutdown;CHANNEL_ID={};SCRIPTPUBKEY={}",
                shutdown.channel_id.to_string(),
                shutdown.scriptpubkey.as_bytes().to_lower_hex_string()
            )),
            Err(_) => str_to_c_string(""),
        },
        39 => match msgs::ClosingSigned::read_from_fixed_length_buffer(&mut payload) {
            Ok(closing_signed) => {
                if sig_check_is_zero(&closing_signed.signature) {
                    return str_to_c_string("");
                }
                let mut result = format!(
                    "MSG_TYPE=closing_signed;CHANNEL_ID={};FEE_SATOSHIS={};SIGNATURE={}",
                    closing_signed.channel_id.to_string(),
                    closing_signed.fee_satoshis.to_string(),
                    closing_signed.signature.to_string()
                );

                if let Some(fee_range) = closing_signed.fee_range {
                    result.push_str(&format!(
                        ";FEE_RANGE_MIN={};FEE_RANGE_MAX={}",
                        fee_range.min_fee_satoshis.to_string(),
                        fee_range.max_fee_satoshis.to_string()
                    ));
                }
                str_to_c_string(&result)
            }
            Err(_) => str_to_c_string(""),
        },
        // Skip the closing_complete message type, since it is not supported by LDK yet.
        40 => std::ptr::null_mut(),
        128 => match msgs::UpdateAddHTLC::read_from_fixed_length_buffer(&mut payload) {
            Ok(update_add_htlc) => {
                let pubkey = match update_add_htlc.onion_routing_packet.public_key {
                    Ok(pk) => pk,
                    Err(_) => return str_to_c_string(""),
                };

                // Rust-lightning doesn't check the onion version on decoding
                // phase, so we do it here to be compatible with other
                // implementations.
                if update_add_htlc.onion_routing_packet.version != 0 {
                    return str_to_c_string("");
                }

                let mut result = format!(
                    "MSG_TYPE=update_add_htlc;CHANNEL_ID={};ID={};AMOUNT={};PAYMENT_HASH={};EXPIRY={};ONION_ROUTING_PACKET=[VERSION={};PUBLIC_KEY={};HOP_DATA={};HMAC={}]",
                    update_add_htlc.channel_id.to_string(),
                    update_add_htlc.htlc_id.to_string(),
                    update_add_htlc.amount_msat.to_string(),
                    update_add_htlc.payment_hash.to_string(),
                    update_add_htlc.cltv_expiry.to_string(),
                    update_add_htlc.onion_routing_packet.version.to_string(),
                    pubkey.to_string(),
                    update_add_htlc.onion_routing_packet.hop_data.to_lower_hex_string(),
                    update_add_htlc.onion_routing_packet.hmac.to_lower_hex_string()
                );
                if let Some(blinded_point) = update_add_htlc.blinding_point {
                    result.push_str(";BLINDED_PATH=");
                    result.push_str(&blinded_point.to_string());
                }
                str_to_c_string(&result)
            }
            Err(DecodeError::UnknownRequiredFeature) => std::ptr::null_mut(),
            Err(_) => str_to_c_string(""),
        },
        _ => str_to_c_string(""),
    }
}

fn sig_check_is_zero(sig: &Signature) -> bool {
    let sig_compact = sig.serialize_compact();
    let r = &sig_compact[..32];
    let s = &sig_compact[32..];
    r.iter().all(|&b| b == 0) || s.iter().all(|&b| b == 0)
}

#[no_mangle]
pub extern "C" fn ldk_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(ptr);
        }
    }
}
