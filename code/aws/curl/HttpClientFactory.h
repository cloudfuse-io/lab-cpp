/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpTypes.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>

namespace Buzz {
namespace Http {

/**
 * Interface and default implementation of client for Http stack
 */
class CustomHttpClientFactory : public Aws::Http::HttpClientFactory {
 public:
  /**
   * Creates a shared_ptr of HttpClient with the relevant settings from
   * clientConfiguration
   */
  std::shared_ptr<Aws::Http::HttpClient> CreateHttpClient(
      const Aws::Client::ClientConfiguration& clientConfiguration) const override;
  /**
   * Creates a shared_ptr of HttpRequest with uri, method, and closure for how to create a
   * response stream.
   */
  std::shared_ptr<Aws::Http::HttpRequest> CreateHttpRequest(
      const Aws::String& uri, Aws::Http::HttpMethod method,
      const Aws::IOStreamFactory& streamFactory) const override;
  /**
   * Creates a shared_ptr of HttpRequest with uri, method, and closure for how to create a
   * response stream.
   */
  std::shared_ptr<Aws::Http::HttpRequest> CreateHttpRequest(
      const Aws::Http::URI& uri, Aws::Http::HttpMethod method,
      const Aws::IOStreamFactory& streamFactory) const override;

  void InitStaticState() override;
  void CleanupStaticState() override;
};

}  // namespace Http
}  // namespace Buzz
