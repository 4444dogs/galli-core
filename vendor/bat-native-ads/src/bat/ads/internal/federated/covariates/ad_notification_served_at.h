/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_FEDERATED_COVARIATES_AD_NOTIFICATION_SERVED_AT_H_
#define BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_FEDERATED_COVARIATES_AD_NOTIFICATION_SERVED_AT_H_

#include <string>

#include "base/time/time.h"
#include "bat/ads/internal/federated/covariate.h"

namespace ads {

class AdNotificationServedAt final : public Covariate {
 public:
  AdNotificationServedAt();
  AdNotificationServedAt(const AdNotificationServedAt&) = delete;
  AdNotificationServedAt& operator=(const AdNotificationServedAt&) = delete;
  ~AdNotificationServedAt() override;

  void SetTime(const base::Time time);

  // Covariate:
  brave_federated::mojom::DataType GetDataType() const override;
  brave_federated::mojom::CovariateType GetCovariateType() const override;
  std::string GetValue() const override;

 private:
  base::Time time_;
};

}  // namespace ads

#endif  // BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_FEDERATED_COVARIATES_AD_NOTIFICATION_SERVED_AT_H_
