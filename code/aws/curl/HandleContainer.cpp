/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "HandleContainer.h"

#include <aws/core/utils/logging/LogMacros.h>

#include <algorithm>
#include <iostream>

namespace Buzz {
namespace Http {

#define NUM_LOCKS CURL_LOCK_DATA_LAST
static std::mutex share_lock[NUM_LOCKS];

static void lock_cb(CURL* handle, curl_lock_data data, curl_lock_access access,
                    void* userptr) {
  share_lock[data].lock();
}

static void unlock_cb(CURL* handle, curl_lock_data data, void* userptr) {
  share_lock[data].unlock();
}

static const char* CURL_HANDLE_CONTAINER_TAG = "CurlHandleContainer";

CurlHandleContainer::CurlHandleContainer(unsigned maxSize, long httpRequestTimeout,
                                         long connectTimeout, bool enableTcpKeepAlive,
                                         unsigned long tcpKeepAliveIntervalMs,
                                         long lowSpeedTime, unsigned long lowSpeedLimit)
    : m_maxPoolSize(maxSize),
      m_httpRequestTimeout(httpRequestTimeout),
      m_connectTimeout(connectTimeout),
      m_enableTcpKeepAlive(enableTcpKeepAlive),
      m_tcpKeepAliveIntervalMs(tcpKeepAliveIntervalMs),
      m_lowSpeedTime(lowSpeedTime),
      m_lowSpeedLimit(lowSpeedLimit),
      m_poolSize(0),
      m_shobject(nullptr) {
  AWS_LOGSTREAM_INFO(CURL_HANDLE_CONTAINER_TAG,
                     "Initializing CurlHandleContainer with size " << maxSize);
}

CurlHandleContainer::~CurlHandleContainer() {
  AWS_LOGSTREAM_INFO(CURL_HANDLE_CONTAINER_TAG, "Cleaning up CurlHandleContainer.");
  for (CURL* handle : m_handleContainer.ShutdownAndWait(m_poolSize)) {
    AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG, "Cleaning up " << handle);
    curl_easy_cleanup(handle);
  }
  if (m_shobject != nullptr) {
    AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG, "Cleaning up shared state");
    curl_share_cleanup(m_shobject);
  }
}

CURL* CurlHandleContainer::AcquireCurlHandle() {
  AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG,
                      "Attempting to acquire curl connection.");

  if (!m_handleContainer.HasResourcesAvailable()) {
    AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG,
                        "No current connections available in pool. Attempting to create "
                        "new connections.");
    CheckAndGrowPool();
  }

  CURL* handle = m_handleContainer.Acquire();
  AWS_LOGSTREAM_INFO(CURL_HANDLE_CONTAINER_TAG,
                     "Connection has been released. Continuing.");
  AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG,
                      "Returning connection handle " << handle);
  return handle;
}

void CurlHandleContainer::ReleaseCurlHandle(CURL* handle) {
  if (handle) {
    curl_easy_reset(handle);
    SetDefaultOptionsOnHandle(handle);
    AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG, "Releasing curl handle " << handle);
    m_handleContainer.Release(handle);
    AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG, "Notified waiting threads.");
  }
}

void CurlHandleContainer::DestroyCurlHandle(CURL* handle) {
  if (!handle) {
    return;
  }

  curl_easy_cleanup(handle);
  {
    std::lock_guard<std::mutex> locker(m_containerLock);
    m_poolSize--;
  }
  AWS_LOGSTREAM_DEBUG(
      CURL_HANDLE_CONTAINER_TAG,
      "Destroy curl handle: " << handle << " and decrease pool size by 1.");
}

bool CurlHandleContainer::CheckAndGrowPool() {
  std::lock_guard<std::mutex> locker(m_containerLock);
  if (m_poolSize < m_maxPoolSize) {
    unsigned multiplier = m_poolSize > 0 ? m_poolSize : 1;
    unsigned amountToAdd = (std::min)(multiplier * 2, m_maxPoolSize - m_poolSize);
    if (m_shobject == nullptr) {
      m_shobject = curl_share_init();
      /// dns resolution typically takes ~10ms
      curl_share_setopt(m_shobject, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
      /// ssl session establishement takes ~100ms
      curl_share_setopt(m_shobject, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
      /// connection establishement is mostly dominated by ssl handshake, so ~100ms
      // curl_share_setopt(m_shobject, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
      /// set lock/unlock of shared state
      curl_share_setopt(m_shobject, CURLSHOPT_LOCKFUNC, lock_cb);
      curl_share_setopt(m_shobject, CURLSHOPT_UNLOCKFUNC, unlock_cb);
    }
    AWS_LOGSTREAM_DEBUG(CURL_HANDLE_CONTAINER_TAG,
                        "attempting to grow pool size by " << amountToAdd);

    unsigned actuallyAdded = 0;
    for (unsigned i = 0; i < amountToAdd; ++i) {
      CURL* curlHandle = curl_easy_init();

      if (curlHandle) {
        SetDefaultOptionsOnHandle(curlHandle);
        curl_easy_setopt(curlHandle, CURLOPT_SHARE, m_shobject);
        m_handleContainer.Release(curlHandle);
        ++actuallyAdded;
      } else {
        AWS_LOGSTREAM_ERROR(CURL_HANDLE_CONTAINER_TAG,
                            "curl_easy_init failed to allocate.");
        break;
      }
    }

    AWS_LOGSTREAM_INFO(CURL_HANDLE_CONTAINER_TAG, "Pool grown by " << actuallyAdded);
    m_poolSize += actuallyAdded;

    return actuallyAdded > 0;
  }

  AWS_LOGSTREAM_INFO(CURL_HANDLE_CONTAINER_TAG,
                     "Pool cannot be grown any further, already at max size.");

  return false;
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