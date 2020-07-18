/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "HandleContainer.h"

#include <aws/core/utils/logging/LogMacros.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <stack>

namespace Buzz {
namespace Http {

// #define NUM_LOCKS CURL_LOCK_DATA_LAST
// static std::mutex share_lock[NUM_LOCKS];

// static void lock_cb(CURL* handle, curl_lock_data data, curl_lock_access access,
//                     void* userptr) {
//   share_lock[data].lock();
// }

// static void unlock_cb(CURL* handle, curl_lock_data data, void* userptr) {
//   share_lock[data].unlock();
// }

// static CURLSH* share_object = []() {
//   auto new_state = curl_share_init();
//   /// dns resolution typically takes ~15ms
//   curl_share_setopt(new_state, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
//   /// ssl session establishement takes ~150ms
//   curl_share_setopt(new_state, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
//   /// connection establishement is mostly dominated by ssl handshake duration, ~180ms
//   /// CURL_LOCK_DATA_CONNECT seems to crash on most libcurl versions (curl/curl #4557)
//   // curl_share_setopt(new_state, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
//   /// set lock/unlock of shared state
//   curl_share_setopt(new_state, CURLSHOPT_LOCKFUNC, lock_cb);
//   curl_share_setopt(new_state, CURLSHOPT_UNLOCKFUNC, unlock_cb);
//   return new_state;
// }();

static const char* CURL_HANDLE_CONTAINER_TAG = "CurlHandleContainer";

namespace {
/// A container that stacks initialized connections for each domain
class DomainHandleCache {
 public:
  CURL* acquire(std::string domain) {
    std::lock_guard<std::mutex> locker(mutex_);
    auto item = containers_.find(domain);
    if (item == containers_.end()) {
      AWS_LOGSTREAM_INFO(CURL_HANDLE_CONTAINER_TAG,
                         "Creating handle container for " << domain);
      item = containers_.emplace(domain, std::vector<CURL*>()).first;
    }
    // check if a connection can be found in the cache, otherwise create a new one
    if (item->second.size() > 0) {
      auto cached_handle = item->second.back();
      item->second.pop_back();
      return cached_handle;
    } else {
      auto new_handle = curl_easy_init();
      AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG,
                          "Creating new handle " << new_handle << " for " << domain);
      return new_handle;
    }
  }

  void release(std::string domain, CURL* handle) {
    std::lock_guard<std::mutex> locker(mutex_);
    containers_[domain].push_back(handle);
  }

  ~DomainHandleCache() {
    for (auto& domain : containers_) {
      for (auto handle : domain.second) {
        curl_easy_cleanup(handle);
      }
    }
  }

 private:
  std::map<std::string, std::vector<CURL*>> containers_;
  std::mutex mutex_;
};
}  // namespace

static DomainHandleCache domain_handles;

CurlHandleContainer::CurlHandleContainer(long httpRequestTimeout, long connectTimeout,
                                         bool enableTcpKeepAlive,
                                         unsigned long tcpKeepAliveIntervalMs,
                                         long lowSpeedTime, unsigned long lowSpeedLimit)
    : m_httpRequestTimeout(httpRequestTimeout),
      m_connectTimeout(connectTimeout),
      m_enableTcpKeepAlive(enableTcpKeepAlive),
      m_tcpKeepAliveIntervalMs(tcpKeepAliveIntervalMs),
      m_lowSpeedTime(lowSpeedTime),
      m_lowSpeedLimit(lowSpeedLimit) {}

CURL* CurlHandleContainer::AcquireCurlHandle(std::string domain) {
  AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG,
                      "Attempting to acquire curl connection.");
  auto handle = domain_handles.acquire(domain);
  SetDefaultOptionsOnHandle(handle);
  AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG, "Handle acquired: " << handle);
  return handle;
}

void CurlHandleContainer::ReleaseCurlHandle(std::string domain, CURL* handle) {
  if (handle) {
    curl_easy_reset(handle);  // reset does not change shares
    AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG, "Releasing curl handle: " << handle);
    domain_handles.release(domain, handle);
  }
}

void CurlHandleContainer::DestroyCurlHandle(CURL* handle) {
  if (!handle) {
    return;
  }
  AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG, "Destroy curl handle: " << handle);
  curl_easy_cleanup(handle);
}

void CurlHandleContainer::SetDefaultOptionsOnHandle(CURL* handle) {
  // for timeouts to work in a multi-threaded context,
  // always turn signals off. This also forces dns queries to
  // not be included in the timeout calculations.
  curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, m_httpRequestTimeout);
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, m_connectTimeout);
  curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, m_lowSpeedLimit);
  curl_easy_setopt(
      handle, CURLOPT_LOW_SPEED_TIME,
      m_lowSpeedTime < 1000 ? (m_lowSpeedTime == 0 ? 0 : 1) : m_lowSpeedTime / 1000);
  curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, m_enableTcpKeepAlive ? 1L : 0L);
  curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, m_tcpKeepAliveIntervalMs / 1000);
  curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, m_tcpKeepAliveIntervalMs / 1000);
#ifdef CURL_HAS_H2
  curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
#endif
}

}  // namespace Http
}  // namespace Buzz