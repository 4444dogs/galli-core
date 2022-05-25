/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_ads/device_id/device_id_impl.h"

#include <sys/socket.h>  // Must be included before ifaddrs.h.
#include <ifaddrs.h>
#include <net/if.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>

#include <map>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace brave_ads {

namespace {

typedef base::RepeatingCallback<bool(const void* bytes, size_t size)>
    IsValidMacAddressCallback;

const char kDiskByUuidDirectoryName[] = "/dev/disk/by-uuid";
const char* const kDeviceNames[] = {
    "sda1", "hda1", "dm-0", "xvda1", "sda2", "hda2", "dm-1", "xvda2",
};
// Fedora 15 uses biosdevname feature where Embedded ethernet uses the
// "em" prefix and PCI cards use the p[0-9]c[0-9] format based on PCI
// slot and card information.
const char* const kNetDeviceNamePrefixes[] = {
    "eth", "em", "en", "wl", "ww", "p0", "p1", "p2",
    "p3",  "p4", "p5", "p6", "p7", "p8", "p9", "wlan"};

// Map from device name to disk uuid
typedef std::map<base::FilePath, base::FilePath> DiskEntries;

std::string GetDiskUuid() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DiskEntries disk_uuids;
  base::FileEnumerator files(base::FilePath(kDiskByUuidDirectoryName),
                             false,  // Recursive.
                             base::FileEnumerator::FILES);
  do {
    base::FilePath file_path = files.Next();
    if (file_path.empty())
      break;

    base::FilePath target_path;
    if (!base::ReadSymbolicLink(file_path, &target_path))
      continue;

    base::FilePath device_name = target_path.BaseName();
    base::FilePath disk_uuid = file_path.BaseName();
    disk_uuids[device_name] = disk_uuid;
  } while (true);

  // Look for first device name matching an entry of |kDeviceNames|.
  std::string result;
  for (size_t i = 0; i < std::size(kDeviceNames); i++) {
    DiskEntries::iterator it = disk_uuids.find(base::FilePath(kDeviceNames[i]));
    if (it != disk_uuids.end()) {
      DVLOG(1) << "Returning uuid: \"" << it->second.value()
               << "\" for device \"" << it->first.value() << "\"";
      result = it->second.value();
      break;
    }
  }

  // Log failure (at most once) for diagnostic purposes.
  static bool error_logged = false;
  if (result.empty() && !error_logged) {
    error_logged = true;
    LOG(ERROR) << "Could not find appropriate disk uuid.";
    for (DiskEntries::iterator it = disk_uuids.begin(); it != disk_uuids.end();
         ++it) {
      LOG(ERROR) << "  DeviceID=" << it->first.value()
                 << ", uuid=" << it->second.value();
    }
  }

  return result;
}

class MacAddressProcessor {
 public:
  explicit MacAddressProcessor(IsValidMacAddressCallback is_valid_mac_address)
      : is_valid_mac_address_(std::move(is_valid_mac_address)) {}

  bool ProcessInterface(struct ifaddrs* ifaddr,
                        const char* const prefixes[],
                        size_t prefixes_count) {
    const int MAC_LENGTH = 6;
    struct ifreq ifinfo;

    memset(&ifinfo, 0, sizeof(ifinfo));
    strncpy(ifinfo.ifr_name, ifaddr->ifa_name, sizeof(ifinfo.ifr_name) - 1);

    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    int result = ioctl(sd, SIOCGIFHWADDR, &ifinfo);
    close(sd);

    if (result != 0)
      return true;

    const char* mac_address =
        static_cast<const char*>(ifinfo.ifr_hwaddr.sa_data);
    if (!is_valid_mac_address_.Run(mac_address, MAC_LENGTH))
      return true;

    if (!IsValidPrefix(ifinfo.ifr_name, prefixes, prefixes_count))
      return true;

    // Got one!
    found_mac_address_ =
        base::ToLowerASCII(base::HexEncode(mac_address, MAC_LENGTH));
    return false;
  }

  std::string mac_address() const { return found_mac_address_; }

 private:
  bool IsValidPrefix(const char* name,
                     const char* const prefixes[],
                     size_t prefixes_count) {
    for (size_t i = 0; i < prefixes_count; i++) {
      if (strncmp(prefixes[i], name, strlen(prefixes[i])) == 0)
        return true;
    }
    return false;
  }

  IsValidMacAddressCallback is_valid_mac_address_;
  std::string found_mac_address_;
};

std::string GetMacAddress(IsValidMacAddressCallback is_valid_mac_address) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  struct ifaddrs* ifaddrs;
  int rv = getifaddrs(&ifaddrs);
  if (rv < 0) {
    PLOG(ERROR) << "getifaddrs failed " << rv;
    return "";
  }

  MacAddressProcessor processor(std::move(is_valid_mac_address));
  for (struct ifaddrs* ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
    bool keep_going = processor.ProcessInterface(
        ifa, kNetDeviceNamePrefixes, std::size(kNetDeviceNamePrefixes));
    if (!keep_going)
      break;
  }
  freeifaddrs(ifaddrs);
  return processor.mac_address();
}

void GetRawDeviceIdImpl(IsValidMacAddressCallback is_valid_mac_address,
                        DeviceId::IdCallback callback) {
  std::string disk_id = GetDiskUuid();
  std::string mac_address = GetMacAddress(std::move(is_valid_mac_address));

  std::string raw_device_id;
  if (!mac_address.empty() && !disk_id.empty()) {
    raw_device_id = mac_address + disk_id;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(raw_device_id)));
}

}  // namespace

// static
void DeviceIdImpl::GetRawDeviceId(IdCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTask(
      FROM_HERE, traits(),
      base::BindOnce(&GetRawDeviceIdImpl,
                     base::BindRepeating(&DeviceIdImpl::IsValidMacAddress),
                     std::move(callback)));
}

}  // namespace brave_ads
