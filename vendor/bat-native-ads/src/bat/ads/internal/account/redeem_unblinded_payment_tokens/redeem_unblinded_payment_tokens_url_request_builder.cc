/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/account/redeem_unblinded_payment_tokens/redeem_unblinded_payment_tokens_url_request_builder.h"

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "bat/ads/internal/account/redeem_unblinded_payment_tokens/redeem_unblinded_payment_tokens_user_data_builder.h"
#include "bat/ads/internal/privacy/challenge_bypass_ristretto_util.h"
#include "bat/ads/internal/privacy/unblinded_payment_tokens/unblinded_payment_token_info.h"
#include "bat/ads/internal/server/confirmations_server_util.h"
#include "bat/ads/internal/server/via_header_util.h"
#include "wrapper.hpp"

namespace ads {

using challenge_bypass_ristretto::TokenPreimage;
using challenge_bypass_ristretto::VerificationKey;
using challenge_bypass_ristretto::VerificationSignature;

RedeemUnblindedPaymentTokensUrlRequestBuilder::
    RedeemUnblindedPaymentTokensUrlRequestBuilder(
        const WalletInfo& wallet,
        const privacy::UnblindedPaymentTokenList& unblinded_payment_tokens,
        const base::Value& user_data)
    : wallet_(wallet),
      unblinded_payment_tokens_(unblinded_payment_tokens),
      user_data_(user_data.Clone()) {
  DCHECK(wallet_.IsValid());
  DCHECK(!unblinded_payment_tokens_.empty());
}

RedeemUnblindedPaymentTokensUrlRequestBuilder::
    ~RedeemUnblindedPaymentTokensUrlRequestBuilder() = default;

// PUT /v2/confirmation/payment/{payment_id}

mojom::UrlRequestPtr RedeemUnblindedPaymentTokensUrlRequestBuilder::Build() {
  mojom::UrlRequestPtr url_request = mojom::UrlRequest::New();
  url_request->url = BuildUrl();
  url_request->headers = BuildHeaders();
  const std::string payload = CreatePayload();
  url_request->content = BuildBody(payload);
  url_request->content_type = "application/json";
  url_request->method = mojom::UrlRequestMethod::kPut;

  return url_request;
}

///////////////////////////////////////////////////////////////////////////////

std::string RedeemUnblindedPaymentTokensUrlRequestBuilder::BuildUrl() const {
  return base::StringPrintf("%s/v2/confirmation/payment/%s",
                            confirmations::server::GetHost().c_str(),
                            wallet_.id.c_str());
}

std::vector<std::string>
RedeemUnblindedPaymentTokensUrlRequestBuilder::BuildHeaders() const {
  std::vector<std::string> headers;

  const std::string via_header = server::BuildViaHeader();
  headers.push_back(via_header);

  const std::string accept_header = "accept: application/json";
  headers.push_back(accept_header);

  return headers;
}

std::string RedeemUnblindedPaymentTokensUrlRequestBuilder::BuildBody(
    const std::string& payload) const {
  DCHECK(!payload.empty());

  base::DictionaryValue dictionary;

  base::Value payment_request_dto = CreatePaymentRequestDTO(payload);
  dictionary.SetKey("paymentCredentials", payment_request_dto.Clone());

  dictionary.SetKey("payload", base::Value(payload));

  dictionary.MergeDictionary(&user_data_);

  std::string json;
  base::JSONWriter::Write(dictionary, &json);

  return json;
}

std::string RedeemUnblindedPaymentTokensUrlRequestBuilder::CreatePayload()
    const {
  base::DictionaryValue payload;
  payload.SetKey("paymentId", base::Value(wallet_.id));

  std::string json;
  base::JSONWriter::Write(payload, &json);

  return json;
}

base::Value
RedeemUnblindedPaymentTokensUrlRequestBuilder::CreatePaymentRequestDTO(
    const std::string& payload) const {
  DCHECK(!payload.empty());

  base::ListValue payment_request_dto;

  for (const auto& unblinded_payment_token : unblinded_payment_tokens_) {
    base::DictionaryValue payment_credential;

    base::Value credential = CreateCredential(unblinded_payment_token, payload);
    payment_credential.SetKey("credential", base::Value(credential.Clone()));

    payment_credential.SetKey(
        "confirmationType",
        base::Value(std::string(unblinded_payment_token.confirmation_type)));

    payment_credential.SetKey(
        "publicKey",
        base::Value(unblinded_payment_token.public_key.encode_base64()));

    payment_request_dto.Append(payment_credential.Clone());
  }

  return payment_request_dto;
}

base::Value RedeemUnblindedPaymentTokensUrlRequestBuilder::CreateCredential(
    const privacy::UnblindedPaymentTokenInfo& unblinded_payment_token,
    const std::string& payload) const {
  DCHECK(!payload.empty());

  base::DictionaryValue credential;

  VerificationKey verification_key =
      unblinded_payment_token.value.derive_verification_key();
  VerificationSignature verification_signature = verification_key.sign(payload);
  if (privacy::ExceptionOccurred()) {
    NOTREACHED();
    return credential;
  }

  const std::string verification_signature_base64 =
      verification_signature.encode_base64();
  if (privacy::ExceptionOccurred()) {
    NOTREACHED();
    return credential;
  }

  TokenPreimage token_preimage = unblinded_payment_token.value.preimage();
  if (privacy::ExceptionOccurred()) {
    NOTREACHED();
    return credential;
  }

  const std::string token_preimage_base64 = token_preimage.encode_base64();
  if (privacy::ExceptionOccurred()) {
    NOTREACHED();
    return credential;
  }

  credential.SetKey("signature", base::Value(verification_signature_base64));
  credential.SetKey("t", base::Value(token_preimage_base64));

  return credential;
}

}  // namespace ads
