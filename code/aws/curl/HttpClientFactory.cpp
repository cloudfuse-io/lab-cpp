/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "HttpClientFactory.h"

#include <aws/core/http/standard/StandardHttpRequest.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <signal.h>

#include "HttpClient.h"

namespace Buzz {
namespace Http {
static bool s_InitCleanupCurlFlag(true);
static bool s_InstallSigPipeHandler(false);

static const char* HTTP_CLIENT_FACTORY_ALLOCATION_TAG = "HttpClientFactory";

static void LogAndSwallowHandler(int signal) {
  switch (signal) {
    case SIGPIPE:
      AWS_LOGSTREAM_ERROR(HTTP_CLIENT_FACTORY_ALLOCATION_TAG, "Received a SIGPIPE error");
      break;
    default:
      AWS_LOGSTREAM_ERROR(HTTP_CLIENT_FACTORY_ALLOCATION_TAG,
                          "Unhandled system SIGNAL error" << signal);
  }
}

/// This http client factory ignores maxConnections as pool is specific to a domain
/// (in order to better re-use ssl connections) and not by client
std::shared_ptr<Aws::Http::HttpClient> CustomHttpClientFactory::CreateHttpClient(
    const Aws::Client::ClientConfiguration& clientConfiguration) const {
  return Aws::MakeShared<Http::CurlHttpClient>(HTTP_CLIENT_FACTORY_ALLOCATION_TAG,
                                               clientConfiguration);
}

std::shared_ptr<Aws::Http::HttpRequest> CustomHttpClientFactory::CreateHttpRequest(
    const Aws::String& uri, Aws::Http::HttpMethod method,
    const Aws::IOStreamFactory& streamFactory) const {
  return CreateHttpRequest(Aws::Http::URI(uri), method, streamFactory);
}

std::shared_ptr<Aws::Http::HttpRequest> CustomHttpClientFactory::CreateHttpRequest(
    const Aws::Http::URI& uri, Aws::Http::HttpMethod method,
    const Aws::IOStreamFactory& streamFactory) const {
  auto request = Aws::MakeShared<Aws::Http::Standard::StandardHttpRequest>(
      HTTP_CLIENT_FACTORY_ALLOCATION_TAG, uri, method);
  request->SetResponseStreamFactory(streamFactory);

  return request;
}

void CustomHttpClientFactory::InitStaticState() {
  if (s_InitCleanupCurlFlag) {
    Http::CurlHttpClient::InitGlobalState();
  }
  if (s_InstallSigPipeHandler) {
    ::signal(SIGPIPE, LogAndSwallowHandler);
  }
}

void CustomHttpClientFactory::CleanupStaticState() {
  if (s_InitCleanupCurlFlag) {
    Http::CurlHttpClient::CleanupGlobalState();
  }
}

}  // namespace Http
}  // namespace Buzz
