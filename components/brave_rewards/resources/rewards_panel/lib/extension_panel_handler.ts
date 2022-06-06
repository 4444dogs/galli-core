/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as mojom from 'gen/brave/components/brave_rewards/common/brave_rewards_panel.mojom.m.js'

export class ExtensionPanelHandlerRemote extends mojom.PanelHandlerRemote {
  constructor () { super() }

  showUI () {}

  closeUI () { window.close() }

  async startRewards () {}

  async getRewardsPanelArgs () {
    const { hash } = location
    location.hash = ''

    if (hash.match(/^#?tour$/i)) {
      return {
        args: { view: mojom.RewardsPanelView.kRewardsTour, data: '' }
      }
    }

    const adaptiveCaptchaMatch = hash.match(/^#?load_adaptive_captcha$/i)
    if (adaptiveCaptchaMatch) {
      return {
        args: { view: mojom.RewardsPanelView.kAdaptiveCaptcha, data: '' }
      }
    }

    const grantMatch = hash.match(/^#?grant_([\s\S]+)$/i)
    if (grantMatch) {
      return {
        args: {
          view: mojom.RewardsPanelView.kGrantCaptcha,
          data: grantMatch[1]
        }
      }
    }

    return { args: { view: mojom.RewardsPanelView.kDefault, data: '' } }
  }
}

export function createExtensionPanelHandler () {
  return new ExtensionPanelHandlerRemote()
}

export function getExtensionString (key: string) {
  // In order to normalize messages across extensions and WebUI, replace all
  // chrome.i18n message placeholders with $N marker patterns. UI components
  // are responsible for replacing these markers with appropriate text or
  // using the markers to build HTML.
  return chrome.i18n.getMessage(key,
    ['$1', '$2', '$3', '$4', '$5', '$6', '$7', '$8', '$9'])
}
