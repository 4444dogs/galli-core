/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/search_engines/private_window_search_engine_provider_service.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url.h"

PrivateWindowSearchEngineProviderService::
PrivateWindowSearchEngineProviderService(Profile* otr_profile)
    : SearchEngineProviderService(otr_profile) {
  DCHECK(otr_profile->IsIncognitoProfile());

  if (ShouldUseExtensionSearchProvider()) {
    UseExtensionSearchProvider();
  } else {
    ConfigureSearchEngineProvider();
  }

  // Monitor normal profile's search engine changing because private window
  // should that search engine provider when alternative search engine isn't
  // used.
  observation_.Observe(original_template_url_service_);
}

PrivateWindowSearchEngineProviderService::
    ~PrivateWindowSearchEngineProviderService() = default;

void PrivateWindowSearchEngineProviderService::
OnUseAlternativeSearchEngineProviderChanged() {
  // If extension search provider is used, user can't change DSE by toggling.
  if (ShouldUseExtensionSearchProvider())
    return;

  ConfigureSearchEngineProvider();
}

void PrivateWindowSearchEngineProviderService::
ConfigureSearchEngineProvider() {
  if (ShouldUseExtensionSearchProvider())
    return;

  UseAlternativeSearchEngineProvider()
      ? ChangeToAlternativeSearchEngineProvider()
      : ChangeToNormalWindowSearchEngineProvider();
}

void PrivateWindowSearchEngineProviderService::OnTemplateURLServiceChanged() {
  if (ShouldUseExtensionSearchProvider()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrivateWindowSearchEngineProviderService::UseExtensionSearchProvider,
                                weak_factory_.GetWeakPtr()));
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrivateWindowSearchEngineProviderService::ConfigureSearchEngineProvider,
                                weak_factory_.GetWeakPtr()));
}

void PrivateWindowSearchEngineProviderService::Shutdown() {
  SearchEngineProviderService::Shutdown();
  observation_.Reset();
}
