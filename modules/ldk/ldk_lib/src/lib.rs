use lightning::bitcoin::bech32::primitives::decode::CheckedHrpstring;
use lightning::bitcoin::bech32::NoChecksum;
use lightning::bitcoin::hex::{Case, DisplayHex};
use lightning::bolt11_invoice::{
    Bolt11Invoice, Bolt11InvoiceDescriptionRef, Bolt11SemanticError, Currency, ParseOrSemanticError,
};
use lightning::offers::invoice_request;
use lightning::offers::offer::{self, Offer};
use lightning::offers::parse::Bolt12ParseError;
use lightning::offers::refund::Refund;
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

            result.push_str(";MIN_FINAL_CLTV_EXPIRY_DELTA=");
            result.push_str(&invoice.min_final_cltv_expiry_delta().to_string());

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

// The invoice_request type in BOLT12 does not currently provide a method to parse a
// Bech32-encoded string into raw bytes, so we replicate the necessary Bech32 handling
// logic from rust-lightning to support this functionality.

// -- lightning/src/offers/parse.rs --
// Used to avoid copying a bech32 string not containing the continuation character (+).
enum Bech32String<'a> {
    Borrowed(&'a str),
    Owned(String),
}

impl<'a> AsRef<str> for Bech32String<'a> {
    fn as_ref(&self) -> &str {
        match self {
            Bech32String::Borrowed(s) => s,
            Bech32String::Owned(s) => s,
        }
    }
}

const INVOICE_REQUEST_BECH32_HRP: &str = "lnr";

fn invoice_request_from_bech32_str(s: &str) -> Result<Vec<u8>, Bolt12ParseError> {
    // Offer encoding may be split by '+' followed by optional whitespace.
    let encoded = match s.split('+').skip(1).next() {
        Some(_) => {
            for chunk in s.split('+') {
                let chunk = chunk.trim_start();
                if chunk.is_empty() || chunk.contains(char::is_whitespace) {
                    return Err(Bolt12ParseError::InvalidContinuation);
                }
            }

            let s: String = s
                .chars()
                .filter(|c| *c != '+' && !c.is_whitespace())
                .collect();
            Bech32String::Owned(s)
        }
        None => Bech32String::Borrowed(s),
    };

    let parsed = CheckedHrpstring::new::<NoChecksum>(encoded.as_ref())?;
    let hrp = parsed.hrp();
    // Compare the lowercase'd iter to allow for all-uppercase HRPs
    if hrp
        .lowercase_char_iter()
        .ne(INVOICE_REQUEST_BECH32_HRP.chars())
    {
        return Err(Bolt12ParseError::InvalidBech32Hrp);
    }

    let data = parsed.byte_iter().collect::<Vec<u8>>();
    Ok(data)
}
// -- end of lightning/src/offers/parse.rs --

#[no_mangle]
pub unsafe extern "C" fn ldk_des_invoice_request(
    input: *const std::os::raw::c_char,
) -> *mut c_char {
    if input.is_null() {
        return str_to_c_string("");
    }

    // Convert C string to Rust string
    let c_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return str_to_c_string(""),
    };

    let bytes = match invoice_request_from_bech32_str(c_str) {
        Ok(bytes) => bytes,
        Err(_) => return str_to_c_string(""),
    };

    match invoice_request::InvoiceRequest::try_from(bytes) {
        Ok(invoice_req) => {
            let mut result = String::new();

            // Since we could parse the `InvoiceRequest` struct, we should be
            // able to parse the `Refund` struct as well.
            let refund = Refund::from_str(c_str).unwrap();

            // offer fields
            result.push_str("CHAINS=");
            invoice_req.chains().iter().for_each(|chain| {
                result.push_str(&format!("{};", chain.to_string()));
            });

            result.push_str("METADATA=");
            if let Some(metadata) = invoice_req.metadata() {
                result.push_str(&metadata.to_hex_string(Case::Lower));
            }

            if let Some(amount) = invoice_req.amount() {
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
            if let Some(description) = invoice_req.description() {
                result.push_str(description.0);
            }

            result.push_str(";FEATURES=");
            let features = invoice_req.offer_features();
            let mut be_flags = features.le_flags().to_vec();
            be_flags.reverse();
            result.push_str(be_flags.to_hex_string(Case::Lower).as_str());

            result.push_str(";ABSOLUTE_EXPIRY=");
            if let Some(absolute_expiry) = invoice_req.absolute_expiry() {
                result.push_str(absolute_expiry.as_secs().to_string().as_str());
            }

            invoice_req
                .paths()
                .iter()
                .enumerate()
                .for_each(|(i, path)| {
                    path.blinded_hops().iter().for_each(|hop| {
                        result.push_str(&format!(";PATH_{}_HOP=", i));
                        result.push_str(hop.blinded_node_id.to_string().as_str());
                    });
                });

            result.push_str(";ISSUER=");
            if let Some(issuer) = invoice_req.issuer() {
                result.push_str(issuer.0);
            }

            result.push_str(";QUANTITY=");
            let quantity = invoice_req.supported_quantity();
            match quantity {
                offer::Quantity::Bounded(n) => result.push_str(&n.to_string()),
                _ => (),
            }

            result.push_str(";ISSUER_ID=");
            if let Some(issuer_id) = invoice_req.issuer_signing_pubkey() {
                result.push_str(&issuer_id.to_string());
            }

            // invoice_request fields
            result.push_str(";INV_REQ_METADATA=");
            result.push_str(&invoice_req.payer_metadata().to_hex_string(Case::Lower));

            result.push_str(";INV_REQ_CHAIN=");
            result.push_str(&invoice_req.chain().to_string());

            result.push_str(";INV_REQ_AMOUNT=");
            if let Some(amount) = invoice_req.amount_msats() {
                result.push_str(&amount.to_string());
            }

            result.push_str(";INV_REQ_FEATURES=");
            let features = invoice_req.invoice_request_features();
            let mut be_flags = features.le_flags().to_vec();
            be_flags.reverse();
            result.push_str(be_flags.to_hex_string(Case::Lower).as_str());

            result.push_str(";INV_REQ_QUANTITY=");
            if let Some(quantity) = invoice_req.quantity() {
                result.push_str(&quantity.to_string());
            }

            result.push_str(";INV_REQ_PAYER_ID=");
            result.push_str(&invoice_req.payer_signing_pubkey().to_string());

            result.push_str(";INV_REQ_PAYER_NOTE=");
            if let Some(payer_note) = invoice_req.payer_note() {
                result.push_str(payer_note.0);
            }

            refund
                .paths()
                .iter()
                .enumerate()
                .for_each(|(i, path)| {
                    path.blinded_hops().iter().for_each(|hop| {
                        result.push_str(&format!(";INV_REQ_PATH_{}_HOP=", i));
                        result.push_str(hop.blinded_node_id.to_string().as_str());
                    });
                });

            result.push_str(";INV_REQ_BIP_353_NAME=");
            if let Some(human_readable) = invoice_req.offer_from_hrn() {
                result.push_str("NAME=");
                result.push_str(human_readable.user());
                result.push_str(";DOMAIN=");
                result.push_str(human_readable.domain());
            }

            result.push_str(";INV_REQ_SIGNATURE=");
            result.push_str(&invoice_req.signature().to_string());

            str_to_c_string(&result)
        }
        Err(_) => str_to_c_string(""),
    }
}

#[no_mangle]
pub extern "C" fn ldk_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(ptr);
        }
    }
}
