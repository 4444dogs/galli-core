/**
 * Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.chromium.chrome.browser.vpn.utils;

import android.app.Activity;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.vpn.BraveVpnNativeWorker;
import org.chromium.chrome.browser.vpn.models.BraveVpnPrefModel;
import org.chromium.chrome.browser.vpn.models.ErrorMessageModel;
import org.chromium.chrome.browser.vpn.utils.BraveVpnPrefUtils;
import org.chromium.chrome.browser.vpn.utils.BraveVpnProfileUtils;
import org.chromium.chrome.browser.vpn.utils.BraveVpnUtils;
import org.chromium.chrome.browser.vpn.utils.InAppPurchaseWrapper;
import org.chromium.ui.widget.Toast;

import java.util.TimeZone;

public class BraveVpnApiResponseUtils {
    public static void queryPurchaseFailed(Activity activity) {
        BraveVpnPrefUtils.setPurchaseToken("");
        BraveVpnPrefUtils.setProductId("");
        BraveVpnPrefUtils.setPurchaseExpiry(0L);
        BraveVpnPrefUtils.setSubscriptionPurchase(false);
        if (BraveVpnProfileUtils.getInstance().isBraveVPNConnected(activity)) {
            BraveVpnProfileUtils.getInstance().stopVpn(activity);
        }
        handleFailure(activity,
                activity.getResources().getString(R.string.purchase_token_verification_failed),
                "Failed to veify purchase token.");
    }

    public static void handleOnGetSubscriberCredential(Activity activity, boolean isSuccess) {
        if (isSuccess) {
            InAppPurchaseWrapper.getInstance().processPurchases(
                    activity, InAppPurchaseWrapper.getInstance().queryPurchases());
            BraveVpnNativeWorker.getInstance().getTimezonesForRegions();
        } else {
            handleFailure(activity,
                    activity.getResources().getString(R.string.vpn_profile_creation_failed),
                    "Failed to get Subscriber credential.");
        }
    }

    public static void handleOnGetTimezonesForRegions(Activity activity,
            BraveVpnPrefModel braveVpnPrefModel, String jsonTimezones, boolean isSuccess) {
        if (isSuccess) {
            String region = BraveVpnUtils.getRegionForTimeZone(
                    jsonTimezones, TimeZone.getDefault().getID());
            if (TextUtils.isEmpty(region)) {
                handleFailure(activity,
                        activity.getResources().getString(R.string.vpn_profile_creation_failed),
                        "Couldn't get matching timezone for : " + TimeZone.getDefault().getID());
                return;
            }

            if (!TextUtils.isEmpty(BraveVpnUtils.selectedServerRegion)
                    && BraveVpnUtils.selectedServerRegion != null) {
                region = BraveVpnUtils.selectedServerRegion.equals(
                                 BraveVpnPrefUtils.PREF_BRAVE_VPN_AUTOMATIC)
                        ? region
                        : BraveVpnUtils.selectedServerRegion;
                BraveVpnUtils.selectedServerRegion = null;
            } else {
                String serverRegion = BraveVpnPrefUtils.getServerRegion();
                region = serverRegion.equals(BraveVpnPrefUtils.PREF_BRAVE_VPN_AUTOMATIC)
                        ? region
                        : serverRegion;
            }

            BraveVpnNativeWorker.getInstance().getHostnamesForRegion(region);
            braveVpnPrefModel.setServerRegion(region);
        } else {
            if (!TextUtils.isEmpty(jsonTimezones) || jsonTimezones != null) {
                ErrorMessageModel errorMessageModel =
                        BraveVpnUtils.getErrorMessageModel(jsonTimezones);
                handleFailure(activity, errorMessageModel.getErrorTitle(),
                        errorMessageModel.getErrorTitle());
            } else {
                handleFailure(activity,
                        activity.getResources().getString(R.string.vpn_profile_creation_failed),
                        "Failed to get timezones for the region.");
            }
        }
    }

    public static Pair<String, String> handleOnGetHostnamesForRegion(Activity activity,
            BraveVpnPrefModel braveVpnPrefModel, String jsonHostNames, boolean isSuccess) {
        Pair<String, String> host = new Pair<String, String>("", "");
        if (isSuccess && braveVpnPrefModel != null) {
            host = BraveVpnUtils.getHostnameForRegion(jsonHostNames);
            BraveVpnNativeWorker.getInstance().getWireguardProfileCredentials(
                    braveVpnPrefModel.getSubscriberCredential(),
                    braveVpnPrefModel.getClientPublicKey(), host.first);
        } else {
            if (!TextUtils.isEmpty(jsonHostNames) || jsonHostNames != null) {
                ErrorMessageModel errorMessageModel =
                        BraveVpnUtils.getErrorMessageModel(jsonHostNames);
                handleFailure(activity, errorMessageModel.getErrorTitle(),
                        errorMessageModel.getErrorTitle());
            } else {
                handleFailure(activity,
                        activity.getResources().getString(R.string.vpn_profile_creation_failed),
                        "Failed to get hostnames for the region.");
            }
        }
        return host;
    }

    private static void handleFailure(Activity activity, String errorTitle, String errorMessage) {
        Toast.makeText(activity, errorTitle + "\n" + errorMessage, Toast.LENGTH_LONG).show();
        BraveVpnUtils.dismissProgressDialog();
    }
}
