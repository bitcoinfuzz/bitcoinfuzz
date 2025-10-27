use lightning::bitcoin::bech32::primitives::decode::{CheckedHrpstringError, ChecksumError};
use lightning::bitcoin::hex::{Case, DisplayHex};
use lightning::bitcoin::secp256k1::ecdsa::Signature;
use lightning::bolt11_invoice::{
    Bolt11Invoice, Bolt11InvoiceDescriptionRef, Bolt11ParseError, Bolt11SemanticError, Currency, ParseOrSemanticError
};
use lightning::io::Cursor;
use lightning::ln::msgs::{self, DecodeError};
use lightning::offers::offer::{self, Offer};
use lightning::util::ser::Readable;
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
        // Some implementations don't check for features when parsing invoices
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::InvalidFeatures)) => {
            std::ptr::null_mut()
        }
        // Implementations have different size limits for the bech32 length
        Err(ParseOrSemanticError::ParseError(Bolt11ParseError::Bech32Error(
            CheckedHrpstringError::Checksum(ChecksumError::CodeLength(_)),
        ))) => {
            std::ptr::null_mut()
        }
        // Only rust-lightning reject invoices with multiple payment secrets
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::MultiplePaymentSecrets)) => {
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
            offer.chains().iter().for_each(|chain| {
                result.push_str(&format!("{};", chain.to_string()));
            });

            result.push_str("METADATA=");
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
                        let code_str = match std::str::from_utf8(&iso4217_code) {
                            Ok(s) => s,
                            Err(_) => "Unknown",
                        };
                        result.push_str(code_str);
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
    let mut cursor = Cursor::new(&data[2..]);

    match msg_type {
        1 => match msgs::WarningMessage::read(&mut cursor) {
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
        17 => match msgs::ErrorMessage::read(&mut cursor) {
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
        18 => match msgs::Ping::read(&mut cursor) {
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
        19 => match msgs::Pong::read(&mut cursor) {
            Ok(pong) => {
                str_to_c_string(format!("MSG_TYPE=pong;IGNORED={}", pong.byteslen).as_str())
            }
            Err(_) => str_to_c_string(""),
        },
        34 => match msgs::FundingCreated::read(&mut cursor) {
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
        35 => match msgs::FundingSigned::read(&mut cursor) {
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
        36 => match msgs::ChannelReady::read(&mut cursor) {
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
