use lightning::bitcoin::hex::{Case, DisplayHex};
use lightning::bitcoin::key::Secp256k1;
use lightning::bitcoin::secp256k1::ecdh::SharedSecret;
use lightning::bitcoin::secp256k1::ecdsa::{RecoverableSignature, Signature};
use lightning::bitcoin::secp256k1::{schnorr, PublicKey, Scalar, SecretKey};
use lightning::bolt11_invoice::{
    Bolt11Invoice, Bolt11InvoiceDescriptionRef, Bolt11SemanticError, Currency,
    ParseOrSemanticError, RawBolt11Invoice,
};
use lightning::ln::inbound_payment::ExpandedKey;
use lightning::ln::msgs::{
    self, DecodeError, OnionErrorPacket, OnionPacket, SocketAddress, UnsignedGossipMessage,
};
use lightning::ln::onion_utils::{self, Hop, OnionDecodeErr};
use lightning::offers::invoice::UnsignedBolt12Invoice;
use lightning::offers::offer::{self, Offer};
use lightning::sign::{NodeSigner, PeerStorageKey, ReceiveAuthKey, Recipient};
use lightning::util::ser::LengthReadable;
use std::{ffi::CStr, str::FromStr};

#[path = "../../../../include/bitcoinfuzz/ffi.rs"]
mod ffi;
use ffi::BfResult;

#[no_mangle]
pub unsafe extern "C" fn ldk_des_invoice(input: *const std::os::raw::c_char) -> BfResult {
    if input.is_null() {
        return BfResult::fail();
    }

    // Convert C string to Rust string
    let c_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return BfResult::fail(),
    };

    match Bolt11Invoice::from_str(c_str) {
        Ok(invoice) => {
            if invoice.currency() != Currency::Bitcoin {
                return BfResult::fail();
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
                // Display replaces control characters, so hex the raw bytes.
                result.push_str(
                    &direct_description
                        .as_inner()
                        .0
                        .as_bytes()
                        .to_hex_string(Case::Lower),
                );
            }

            result.push_str(";METADATA=");
            if let Some(metadata) = invoice.payment_metadata() {
                result.push_str(&metadata.to_hex_string(Case::Lower));
            }

            let invoice_payee_pub_key = invoice.get_payee_pub_key();

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

            BfResult::ok(result)
        }
        // Handle invoices without payment secrets by returning null
        // This is needed because some Lightning implementations don't require payment secrets,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::NoPaymentSecret)) => {
            BfResult::skip()
        }
        // Handle invoices with multiple payment hashes by returning null
        // This is needed because some Lightning implementations don't require payment to have only one hash,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::MultiplePaymentHashes)) => {
            BfResult::skip()
        }
        // Handle invoices with multiple descriptions hashes by returning null
        // This is needed because some Lightning implementations don't require to have only one description,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::MultipleDescriptions)) => {
            BfResult::skip()
        }
        Err(_) => BfResult::fail(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn ldk_des_offer(input: *const std::os::raw::c_char) -> BfResult {
    if input.is_null() {
        return BfResult::fail();
    }

    // Convert C string to Rust string
    let c_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return BfResult::fail(),
    };

    match Offer::from_str(c_str) {
        Ok(offer) => {
            let mut result = String::new();

            // chains() already yields mainnet when offer_chains is absent, so an
            // empty list means offer_chains is present but empty. BOLT 12 rejects
            // that, but LDK's parser accepts it, so reject it here.
            if offer.chains().is_empty() {
                return BfResult::fail();
            }

            result.push_str("CHAINS=");
            offer.chains().iter().enumerate().for_each(|(i, chain)| {
                if i > 0 {
                    result.push_str(";");
                }
                result.push_str(&format!("{}", chain.to_string()));
            });

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
                        result.push_str(&iso4217_code.as_bytes().to_hex_string(Case::Lower));
                    }
                }
            }

            result.push_str(";DESCRIPTION=");
            if let Some(description) = offer.description() {
                result.push_str(&description.0.as_bytes().to_hex_string(Case::Lower));
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
                result.push_str(&issuer.0.as_bytes().to_hex_string(Case::Lower));
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

            BfResult::ok(result)
        }
        Err(_) => BfResult::fail(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn ldk_parse_p2p_lightning_message(data: *const u8, len: usize) -> BfResult {
    let data = std::slice::from_raw_parts(data, len);
    if data.len() < 2 {
        return BfResult::fail();
    }

    let msg_type = u16::from_be_bytes([data[0], data[1]]);
    let mut payload = &data[2..];

    match msg_type {
        1 => match msgs::WarningMessage::read_from_fixed_length_buffer(&mut payload) {
            Ok(warning) => BfResult::ok(format!(
                "MSG_TYPE=warning;CHANNEL_ID={};DATA={}",
                warning.channel_id.0.to_hex_string(Case::Lower),
                warning.data.as_bytes().to_hex_string(Case::Lower)
            )),
            // Rust-lightning try to parse the error data as a UTF-8 string.
            // However, other implementations like LND and C-lightning, do not do this.
            Err(DecodeError::InvalidValue) => BfResult::skip(),
            Err(_) => BfResult::fail(),
        },
        16 => match msgs::Init::read_from_fixed_length_buffer(&mut payload) {
            Ok(init) => {
                let mut flags = init.features.le_flags().to_vec();
                flags.reverse();

                // Strip the leading zeros to maintain compatibility with LND
                // which strips leading zeros from the features type by default
                // similar to: https://github.com/bitcoinfuzz/bitcoinfuzz/issues/291
                while flags.first() == Some(&0) && flags.len() > 0 {
                    flags.remove(0);
                }

                let mut result = format!("MSG_TYPE=init;FEATURES={}", flags.to_lower_hex_string());

                if let Some(networks) = init.networks {
                    result.push_str(";NETWORKS=");
                    if !networks.is_empty() {
                        networks.iter().for_each(|network| {
                            result.push_str(&format!("{},", network.to_string()));
                        });
                    }
                }

                if let Some(address) = init.remote_network_address {
                    result.push_str(";REMOTE_NETWORK_ADDRESS=");
                    match address {
                        SocketAddress::TcpIpV6 { addr, port } => {
                            // Use hex representation to avoid differences between c-lightning and rust-lightning formats for IPV6
                            result.push_str(&format!(
                                "{}:{}",
                                addr.to_hex_string(Case::Lower),
                                port
                            ));
                        }
                        SocketAddress::OnionV2(_) => {
                            // Skip OnionV2 since it's deprecated and LDK doesn't fully parse it
                            return BfResult::skip();
                        }
                        other => result.push_str(&other.to_string()),
                    }
                }
                return BfResult::ok(result);
            }
            Err(_) => BfResult::fail(),
        },
        17 => match msgs::ErrorMessage::read_from_fixed_length_buffer(&mut payload) {
            Ok(error) => BfResult::ok(format!(
                "MSG_TYPE=error;CHANNEL_ID={};DATA={}",
                error.channel_id.0.to_hex_string(Case::Lower),
                error.data.as_bytes().to_hex_string(Case::Lower)
            )),
            // Rust-lightning try to parse the error data as a UTF-8 string.
            // However, other implementations like LND and C-lightning, do not do this.
            Err(DecodeError::InvalidValue) => BfResult::skip(),
            Err(_) => BfResult::fail(),
        },
        18 => match msgs::Ping::read_from_fixed_length_buffer(&mut payload) {
            Ok(ping) => {
                if ping.ponglen >= 65532 {
                    BfResult::fail()
                } else {
                    BfResult::ok(format!(
                        "MSG_TYPE=ping;NUM_PONG_BYTES={};IGNORED={}",
                        ping.ponglen, ping.byteslen
                    ))
                }
            }
            Err(_) => BfResult::fail(),
        },
        19 => match msgs::Pong::read_from_fixed_length_buffer(&mut payload) {
            Ok(pong) => BfResult::ok(format!("MSG_TYPE=pong;IGNORED={}", pong.byteslen)),
            Err(_) => BfResult::fail(),
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
                    while flags.first() == Some(&0) && flags.len() > 0 {
                        flags.remove(0);
                    }

                    let hex_str = flags.to_lower_hex_string();
                    s.push_str(&hex_str);
                }
                BfResult::ok(s)
            }
            Err(_) => BfResult::fail(),
        },
        34 => match msgs::FundingCreated::read_from_fixed_length_buffer(&mut payload) {
            Ok(funding_created) => {
                if sig_check_is_zero(&funding_created.signature) {
                    return BfResult::fail();
                }
                BfResult::ok(format!("MSG_TYPE=funding_created;TEMPORARY_CHANNEL_ID={};FUNDING_TXID={};FUNDING_OUTPUT_INDEX={};SIGNATURE={}", 
                funding_created.temporary_channel_id,
                funding_created.funding_txid.to_string(),
                funding_created.funding_output_index,
                funding_created.signature.to_string()
            ))
            }
            Err(DecodeError::UnknownRequiredFeature) => BfResult::skip(),
            Err(_) => BfResult::fail(),
        },
        35 => match msgs::FundingSigned::read_from_fixed_length_buffer(&mut payload) {
            Ok(funding_signed) => {
                if sig_check_is_zero(&funding_signed.signature) {
                    return BfResult::fail();
                }
                BfResult::ok(format!(
                    "MSG_TYPE=funding_signed;CHANNEL_ID={};SIGNATURE={}",
                    funding_signed.channel_id.to_string(),
                    funding_signed.signature.to_string()
                ))
            }
            Err(_) => BfResult::fail(),
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
                BfResult::ok(result)
            }
            Err(_) => BfResult::fail(),
        },
        38 => match msgs::Shutdown::read_from_fixed_length_buffer(&mut payload) {
            Ok(shutdown) => BfResult::ok(format!(
                "MSG_TYPE=shutdown;CHANNEL_ID={};SCRIPTPUBKEY={}",
                shutdown.channel_id.to_string(),
                shutdown.scriptpubkey.as_bytes().to_lower_hex_string()
            )),
            Err(_) => BfResult::fail(),
        },
        39 => match msgs::ClosingSigned::read_from_fixed_length_buffer(&mut payload) {
            Ok(closing_signed) => {
                if sig_check_is_zero(&closing_signed.signature) {
                    return BfResult::fail();
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
                BfResult::ok(result)
            }
            Err(_) => BfResult::fail(),
        },
        // Skip the closing_complete message type, since it is not supported by LDK yet.
        40 => BfResult::skip(),
        128 => match msgs::UpdateAddHTLC::read_from_fixed_length_buffer(&mut payload) {
            Ok(update_add_htlc) => {
                let pubkey = match update_add_htlc.onion_routing_packet.public_key {
                    Ok(pk) => pk,
                    Err(_) => return BfResult::fail(),
                };

                // Rust-lightning doesn't check the onion version on decoding
                // phase, so we do it here to be compatible with other
                // implementations.
                if update_add_htlc.onion_routing_packet.version != 0 {
                    return BfResult::fail();
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
                BfResult::ok(result)
            }
            Err(DecodeError::UnknownRequiredFeature) => BfResult::skip(),
            Err(_) => BfResult::fail(),
        },
        130 => match msgs::UpdateFulfillHTLC::read_from_fixed_length_buffer(&mut payload) {
            Ok(update_fulfill_htlc) => {
                let result = format!(
                    "MSG_TYPE=update_fulfill_htlc;CHANNEL_ID={};ID={};PAYMENT_PREIMAGE={}",
                    update_fulfill_htlc.channel_id,
                    update_fulfill_htlc.htlc_id,
                    update_fulfill_htlc.payment_preimage.0.to_lower_hex_string(),
                );

                BfResult::ok(result)
            }
            Err(DecodeError::UnknownRequiredFeature) => BfResult::skip(),
            Err(_) => BfResult::fail(),
        },
        131 => match msgs::UpdateFailHTLC::read_from_fixed_length_buffer(&mut payload) {
            Ok(update_fail_htlc) => {
                let onion_reason = OnionErrorPacket::from(update_fail_htlc.clone());
                let result = format!(
                    "MSG_TYPE=update_fail_htlc;CHANNEL_ID={};ID={};REASON={}",
                    update_fail_htlc.channel_id,
                    update_fail_htlc.htlc_id,
                    onion_reason.data.to_lower_hex_string()
                );

                BfResult::ok(result)
            }
            Err(DecodeError::UnknownRequiredFeature) => BfResult::skip(),
            Err(_) => BfResult::fail(),
        },
        135 => match msgs::UpdateFailMalformedHTLC::read_from_fixed_length_buffer(&mut payload) {
            Ok(update_fail_malformed_htlc) => {
                let result = format!(
                    "MSG_TYPE=update_fail_malformed_htlc;CHANNEL_ID={};ID={};FAILURE_CODE={}",
                    update_fail_malformed_htlc.channel_id,
                    update_fail_malformed_htlc.htlc_id,
                    update_fail_malformed_htlc.failure_code
                );

                BfResult::ok(result)
            }
            Err(DecodeError::UnknownRequiredFeature) => BfResult::skip(),
            Err(_) => BfResult::fail(),
        },
        _ => BfResult::fail(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn ldk_decode_onion(data: *const u8, len: usize) -> BfResult {
    struct TestEcdhSigner {
        node_secret: SecretKey,
    }
    impl NodeSigner for TestEcdhSigner {
        fn ecdh(
            &self,
            _recipient: Recipient,
            other_key: &PublicKey,
            tweak: Option<&Scalar>,
        ) -> Result<SharedSecret, ()> {
            let mut node_secret = self.node_secret.clone();
            if let Some(tweak) = tweak {
                node_secret = self.node_secret.mul_tweak(tweak).map_err(|_| ())?;
            }
            Ok(SharedSecret::new(other_key, &node_secret))
        }
        fn get_expanded_key(&self) -> ExpandedKey {
            unreachable!()
        }
        fn get_node_id(&self, _recipient: Recipient) -> Result<PublicKey, ()> {
            unreachable!()
        }
        fn sign_invoice(
            &self,
            _invoice: &RawBolt11Invoice,
            _recipient: Recipient,
        ) -> Result<RecoverableSignature, ()> {
            unreachable!()
        }
        fn get_peer_storage_key(&self) -> PeerStorageKey {
            unreachable!()
        }
        // TODO: This a stub key for now since the onion custom mutator
        // currently can't generate valid blinded path onions.
        fn get_receive_auth_key(&self) -> ReceiveAuthKey {
            ReceiveAuthKey([41; 32])
        }
        fn sign_bolt12_invoice(
            &self,
            _invoice: &UnsignedBolt12Invoice,
        ) -> Result<schnorr::Signature, ()> {
            unreachable!()
        }
        fn sign_gossip_message(&self, _msg: UnsignedGossipMessage) -> Result<Signature, ()> {
            unreachable!()
        }
        fn sign_message(&self, _: &[u8]) -> Result<String, ()> {
            unreachable!()
        }
    }

    let data = std::slice::from_raw_parts(data, len);

    if data.len() < 32 {
        return BfResult::fail();
    }
    let Ok(private_key) = SecretKey::from_slice(&data[0..32]) else {
        return BfResult::fail();
    };
    let mut data = &data[32..];

    let Ok(onion_packet) = OnionPacket::read_from_fixed_length_buffer(&mut data) else {
        return BfResult::fail();
    };
    let Ok(onion_pubkey) = onion_packet.public_key else {
        return BfResult::fail();
    };
    if onion_packet.version != 0 {
        return BfResult::fail();
    }
    let node_signer = TestEcdhSigner {
        node_secret: private_key,
    };

    let decoded_hop = match onion_utils::decode_next_payment_hop(
        Recipient::Node,
        &onion_pubkey,
        &onion_packet.hop_data,
        onion_packet.hmac,
        None,
        None,
        &node_signer,
    ) {
        Ok(decoded_hop) => decoded_hop,
        Err(error) => match error {
            // Handle cases where rust-lightning enforces restrictions not
            // present in other implementations. We return early for:
            //
            // 1. `amt` or `total_msat` exceeding 21 million BTC.
            // 2. `tlv_len.0 < 2` (legacy onion packets).
            // 3. Non-blinded receive hop payloads containing
            //    `encrypted_tlvs_opt`, `total_msat`, or `invoice_request`.
            // 4. Non-blinded forward hop payloads containing
            //    `payment_data`, `payment_metadata`, `encrypted_tlvs_opt`,
            //    `total_msat`, or `invoice_request`.
            //
            // Cases 3 and 4 are rust-lightning-specific restrictions not
            // mandated by BOLT-04 since the addition of blinded paths:
            // https://github.com/lightning/bolts/pull/1303
            OnionDecodeErr::Relay { err_msg, .. }
                if err_msg == "Should be skipped by bitcoinfuzz" =>
            {
                return BfResult::skip();
            }
            _ => {
                return BfResult::fail();
            }
        },
    };

    match decoded_hop {
        Hop::Forward {
            next_hop_data,
            shared_secret,
            next_hop_hmac,
            new_packet_bytes,
        } => {
            let secp_ctx = Secp256k1::new();
            let Ok(next_packet_pubkey) = onion_utils::next_hop_pubkey(
                &secp_ctx,
                onion_pubkey,
                &shared_secret.secret_bytes(),
            ) else {
                return BfResult::fail();
            };
            let result = format!(
                "AMT_TO_FORWARD={};SHORT_CHANNEL_ID={};OUTGOING_CLTV_VALUE={};NEXT_HMAC={};NEXT_VERSION={};NEXT_PUBLIC_KEY={};NEXT_HOP_PAYLOADS={}",
                next_hop_data.amt_to_forward,
                next_hop_data.short_channel_id,
                next_hop_data.outgoing_cltv_value,
                next_hop_hmac.to_lower_hex_string(),
                onion_packet.version,
                next_packet_pubkey,
                new_packet_bytes.to_lower_hex_string()
                );

            BfResult::ok(result)
        }
        Hop::Receive {
            hop_data,
            shared_secret: _,
        } => {
            let mut result = format!(
                "AMT_TO_FORWARD={};OUTGOING_CLTV_VALUE={}",
                hop_data.sender_intended_htlc_amt_msat, hop_data.cltv_expiry_height,
            );

            if let Some(preimage) = hop_data.keysend_preimage {
                result.push_str(&format!(
                    ";KEYSEND_PREIMAGE={}",
                    preimage.0.to_lower_hex_string()
                ));
            }

            if let Some(payment_data) = hop_data.payment_data {
                result.push_str(&format!(
                    ";PAYMENT_SECRET={}",
                    payment_data.payment_secret.0.to_lower_hex_string()
                ));
                result.push_str(&format!(
                    ";TOTAL_MSAT={}",
                    payment_data.total_msat.to_string()
                ));
            }

            if let Some(metadata) = hop_data.payment_metadata {
                result.push_str(&format!(
                    ";PAYMENT_METADATA={}",
                    metadata.to_lower_hex_string()
                ));
            }

            BfResult::ok(result)
        }
        // TODO: Currently the custom mutator can't generate any valid onions
        // of these types, and if ever does it will trigger a crash.
        Hop::TrampolineForward { .. } => BfResult::ok("trampoline_forward"),
        Hop::TrampolineBlindedForward { .. } => BfResult::ok("trampoline_blinded_forward"),
        Hop::BlindedForward { .. } => BfResult::ok("blinded_forward"),
        Hop::BlindedReceive { .. } => BfResult::ok("blinded_receive"),
        Hop::TrampolineReceive { .. } => BfResult::ok("trampoline_receive"),
        Hop::TrampolineBlindedReceive { .. } => BfResult::ok("trampoline_blinded_receive"),
        Hop::Dummy { .. } => BfResult::ok("dummy_received"),
    }
}

fn sig_check_is_zero(sig: &Signature) -> bool {
    let sig_compact = sig.serialize_compact();
    let r = &sig_compact[..32];
    let s = &sig_compact[32..];
    r.iter().all(|&b| b == 0) || s.iter().all(|&b| b == 0)
}
