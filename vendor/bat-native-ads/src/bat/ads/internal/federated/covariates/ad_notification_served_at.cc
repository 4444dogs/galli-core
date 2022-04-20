/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/federated/covariates/ad_notification_served_at.h"

#include "bat/ads/internal/federated/covariates_util.h"

namespace ads {

namespace {
constexpr int kMissingValue = -1;
}  // namespace

AdNotificationServedAt::AdNotificationServedAt() = default;

AdNotificationServedAt::~AdNotificationServedAt() = default;

void AdNotificationServedAt::SetTime(const base::Time time) {
  time_ = time;
}

brave_federated::mojom::DataType AdNotificationServedAt::GetDataType() const {
  return brave_federated::mojom::DataType::kDouble;
}

brave_federated::mojom::CovariateType AdNotificationServedAt::GetCovariateType()
    const {
  return brave_federated::mojom::CovariateType::kAdNotificationServedAt;
}

std::string AdNotificationServedAt::GetValue() const {
  if (time_.is_null()) {
    return ToString(kMissingValue);
  }

  return ToString(time_);
}

}  // namespace ads
