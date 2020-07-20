/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "HttpClient.h"

#include <aws/core/Region.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/standard/StandardHttpResponse.h>
#include <aws/core/monitoring/HttpClientMetrics.h>
#include <aws/core/utils/DateTime.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/ratelimiter/RateLimiterInterface.h>

#include <algorithm>
#include <cassert>

#include "logger.h"

namespace Buzz {
namespace Http {

struct CurlWriteCallbackContext {
  CurlWriteCallbackContext(const CurlHttpClient* client, Aws::Http::HttpRequest* request,
                           Aws::Http::HttpResponse* response,
                           Aws::Utils::RateLimits::RateLimiterInterface* rateLimiter)
      : m_client(client),
        m_request(request),
        m_response(response),
        m_rateLimiter(rateLimiter),
        m_numBytesResponseReceived(0) {}

  const CurlHttpClient* m_client;
  Aws::Http::HttpRequest* m_request;
  Aws::Http::HttpResponse* m_response;
  Aws::Utils::RateLimits::RateLimiterInterface* m_rateLimiter;
  int64_t m_numBytesResponseReceived;
};

struct CurlReadCallbackContext {
  CurlReadCallbackContext(const CurlHttpClient* client, Aws::Http::HttpRequest* request,
                          Aws::Utils::RateLimits::RateLimiterInterface* limiter)
      : m_client(client), m_rateLimiter(limiter), m_request(request) {}

  const CurlHttpClient* m_client;
  Aws::Utils::RateLimits::RateLimiterInterface* m_rateLimiter;
  Aws::Http::HttpRequest* m_request;
};

static const char* CURL_HTTP_CLIENT_TAG = "CurlHttpClient";

static size_t WriteData(char* ptr, size_t size, size_t nmemb, void* userdata) {
  if (ptr) {
    CurlWriteCallbackContext* context =
        reinterpret_cast<CurlWriteCallbackContext*>(userdata);

    const CurlHttpClient* client = context->m_client;
    if (!client->ContinueRequest(*context->m_request) ||
        !client->IsRequestProcessingEnabled()) {
      return 0;
    }

    Aws::Http::HttpResponse* response = context->m_response;
    size_t sizeToWrite = size * nmemb;
    if (context->m_rateLimiter) {
      context->m_rateLimiter->ApplyAndPayForCost(static_cast<int64_t>(sizeToWrite));
    }

    response->GetResponseBody().write(ptr, static_cast<std::streamsize>(sizeToWrite));
    auto& receivedHandler = context->m_request->GetDataReceivedEventHandler();
    if (receivedHandler) {
      receivedHandler(context->m_request, context->m_response,
                      static_cast<long long>(sizeToWrite));
    }

    AWS_LOGSTREAM_TRACE(CURL_HTTP_CLIENT_TAG,
                        sizeToWrite << " bytes written to response.");
    context->m_numBytesResponseReceived += sizeToWrite;
    return sizeToWrite;
  }
  return 0;
}

static size_t WriteHeader(char* ptr, size_t size, size_t nmemb, void* userdata) {
  if (ptr) {
    AWS_LOGSTREAM_TRACE(CURL_HTTP_CLIENT_TAG, ptr);
    CurlWriteCallbackContext* context =
        reinterpret_cast<CurlWriteCallbackContext*>(userdata);
    Aws::Http::HttpResponse* response = context->m_response;
    Aws::String headerLine(ptr);
    Aws::Vector<Aws::String> keyValuePair =
        Aws::Utils::StringUtils::Split(headerLine, ':', 2);

    if (keyValuePair.size() == 2) {
      // call receive handler with size 0 on first header
      if (response->GetHeaders().size() == 0) {
        auto& receivedHandler = context->m_request->GetDataReceivedEventHandler();
        if (receivedHandler) {
          receivedHandler(context->m_request, context->m_response,
                          static_cast<long long>(0));
        }
      }
      // parse header into response
      response->AddHeader(Aws::Utils::StringUtils::Trim(keyValuePair[0].c_str()),
                          Aws::Utils::StringUtils::Trim(keyValuePair[1].c_str()));
    }

    return size * nmemb;
  }
  return 0;
}

static size_t ReadBody(char* ptr, size_t size, size_t nmemb, void* userdata) {
  CurlReadCallbackContext* context = reinterpret_cast<CurlReadCallbackContext*>(userdata);
  if (context == nullptr) {
    return 0;
  }

  const CurlHttpClient* client = context->m_client;
  if (!client->ContinueRequest(*context->m_request) ||
      !client->IsRequestProcessingEnabled()) {
    return CURL_READFUNC_ABORT;
  }

  Aws::Http::HttpRequest* request = context->m_request;
  const std::shared_ptr<Aws::IOStream>& ioStream = request->GetContentBody();

  const size_t amountToRead = size * nmemb;
  if (ioStream != nullptr && amountToRead > 0) {
    ioStream->read(ptr, amountToRead);
    size_t amountRead = static_cast<size_t>(ioStream->gcount());
    auto& sentHandler = request->GetDataSentEventHandler();
    if (sentHandler) {
      sentHandler(request, static_cast<long long>(amountRead));
    }
    if (context->m_rateLimiter) {
      context->m_rateLimiter->ApplyAndPayForCost(static_cast<int64_t>(amountRead));
    }

    return amountRead;
  }

  return 0;
}

static size_t SeekBody(void* userdata, curl_off_t offset, int origin) {
  CurlReadCallbackContext* context = reinterpret_cast<CurlReadCallbackContext*>(userdata);
  if (context == nullptr) {
    return CURL_SEEKFUNC_FAIL;
  }

  const CurlHttpClient* client = context->m_client;
  if (!client->ContinueRequest(*context->m_request) ||
      !client->IsRequestProcessingEnabled()) {
    return CURL_SEEKFUNC_FAIL;
  }

  Aws::Http::HttpRequest* request = context->m_request;
  const std::shared_ptr<Aws::IOStream>& ioStream = request->GetContentBody();

  std::ios_base::seekdir dir;
  switch (origin) {
    case SEEK_SET:
      dir = std::ios_base::beg;
      break;
    case SEEK_CUR:
      dir = std::ios_base::cur;
      break;
    case SEEK_END:
      dir = std::ios_base::end;
      break;
    default:
      return CURL_SEEKFUNC_FAIL;
  }

  ioStream->clear();
  ioStream->seekg(offset, dir);
  if (ioStream->fail()) {
    return CURL_SEEKFUNC_CANTSEEK;
  }

  return CURL_SEEKFUNC_OK;
}

void SetOptCodeForHttpMethod(CURL* requestHandle,
                             const std::shared_ptr<Aws::Http::HttpRequest>& request) {
  switch (request->GetMethod()) {
    case Aws::Http::HttpMethod::HTTP_GET:
      curl_easy_setopt(requestHandle, CURLOPT_HTTPGET, 1L);
      break;
    case Aws::Http::HttpMethod::HTTP_POST:
      if (request->HasHeader(Aws::Http::CONTENT_LENGTH_HEADER) &&
          request->GetHeaderValue(Aws::Http::CONTENT_LENGTH_HEADER) == "0") {
        curl_easy_setopt(requestHandle, CURLOPT_CUSTOMREQUEST, "POST");
      } else {
        curl_easy_setopt(requestHandle, CURLOPT_POST, 1L);
      }
      break;
    case Aws::Http::HttpMethod::HTTP_PUT:
      if ((!request->HasHeader(Aws::Http::CONTENT_LENGTH_HEADER) ||
           request->GetHeaderValue(Aws::Http::CONTENT_LENGTH_HEADER) == "0") &&
          !request->HasHeader(Aws::Http::TRANSFER_ENCODING_HEADER)) {
        curl_easy_setopt(requestHandle, CURLOPT_CUSTOMREQUEST, "PUT");
      } else {
        curl_easy_setopt(requestHandle, CURLOPT_PUT, 1L);
      }
      break;
    case Aws::Http::HttpMethod::HTTP_HEAD:
      curl_easy_setopt(requestHandle, CURLOPT_HTTPGET, 1L);
      curl_easy_setopt(requestHandle, CURLOPT_NOBODY, 1L);
      break;
    case Aws::Http::HttpMethod::HTTP_PATCH:
      if ((!request->HasHeader(Aws::Http::CONTENT_LENGTH_HEADER) ||
           request->GetHeaderValue(Aws::Http::CONTENT_LENGTH_HEADER) == "0") &&
          !request->HasHeader(Aws::Http::TRANSFER_ENCODING_HEADER)) {
        curl_easy_setopt(requestHandle, CURLOPT_CUSTOMREQUEST, "PATCH");
      } else {
        curl_easy_setopt(requestHandle, CURLOPT_POST, 1L);
        curl_easy_setopt(requestHandle, CURLOPT_CUSTOMREQUEST, "PATCH");
      }

      break;
    case Aws::Http::HttpMethod::HTTP_DELETE:
      curl_easy_setopt(requestHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
      break;
    default:
      assert(0);
      curl_easy_setopt(requestHandle, CURLOPT_CUSTOMREQUEST, "GET");
      break;
  }
}

std::atomic<bool> CurlHttpClient::isInit(false);

void CurlHttpClient::InitGlobalState() {
  if (!isInit) {
    auto curlVersionData = curl_version_info(CURLVERSION_NOW);
    AWS_LOGSTREAM_INFO(CURL_HTTP_CLIENT_TAG,
                       "Initializing Curl library with version: "
                           << curlVersionData->version
                           << ", ssl version: " << curlVersionData->ssl_version);
    isInit = true;
    curl_global_init(CURL_GLOBAL_ALL);
  }
}

void CurlHttpClient::CleanupGlobalState() { curl_global_cleanup(); }

Aws::String CurlInfoTypeToString(curl_infotype type) {
  switch (type) {
    case CURLINFO_TEXT:
      return "Text";

    case CURLINFO_HEADER_IN:
      return "HeaderIn";

    case CURLINFO_HEADER_OUT:
      return "HeaderOut";

    case CURLINFO_DATA_IN:
      return "DataIn";

    case CURLINFO_DATA_OUT:
      return "DataOut";

    case CURLINFO_SSL_DATA_IN:
      return "SSLDataIn";

    case CURLINFO_SSL_DATA_OUT:
      return "SSLDataOut";

    default:
      return "Unknown";
  }
}

int CurlDebugCallback(CURL* handle, curl_infotype type, char* data, size_t size,
                      void* userptr) {
  AWS_UNREFERENCED_PARAM(handle);
  AWS_UNREFERENCED_PARAM(userptr);

  if (type == CURLINFO_SSL_DATA_IN || type == CURLINFO_SSL_DATA_OUT) {
    AWS_LOGSTREAM_DEBUG("CURL",
                        "(" << CurlInfoTypeToString(type) << ") " << size << "bytes");
  } else {
    Aws::String debugString(data, size);
    AWS_LOGSTREAM_DEBUG("CURL", "(" << CurlInfoTypeToString(type) << ") " << debugString);
  }

  return 0;
}

CurlHttpClient::CurlHttpClient(const Aws::Client::ClientConfiguration& clientConfig)
    : Base(),
      m_curlHandleContainer(
          clientConfig.httpRequestTimeoutMs, clientConfig.connectTimeoutMs,
          clientConfig.enableTcpKeepAlive, clientConfig.tcpKeepAliveIntervalMs,
          clientConfig.requestTimeoutMs, clientConfig.lowSpeedLimit),
      m_isUsingProxy(!clientConfig.proxyHost.empty()),
      m_proxyUserName(clientConfig.proxyUserName),
      m_proxyPassword(clientConfig.proxyPassword),
      m_proxyScheme(Aws::Http::SchemeMapper::ToString(clientConfig.proxyScheme)),
      m_proxyHost(clientConfig.proxyHost),
      m_proxySSLCertPath(clientConfig.proxySSLCertPath),
      m_proxySSLCertType(clientConfig.proxySSLCertType),
      m_proxySSLKeyPath(clientConfig.proxySSLKeyPath),
      m_proxySSLKeyType(clientConfig.proxySSLKeyType),
      m_proxyKeyPasswd(clientConfig.proxySSLKeyPassword),
      m_proxyPort(clientConfig.proxyPort),
      m_verifySSL(clientConfig.verifySSL),
      m_caPath(clientConfig.caPath),
      m_caFile(clientConfig.caFile),
      m_disableExpectHeader(clientConfig.disableExpectHeader) {
  if (clientConfig.followRedirects == Aws::Client::FollowRedirectsPolicy::NEVER ||
      (clientConfig.followRedirects == Aws::Client::FollowRedirectsPolicy::DEFAULT &&
       clientConfig.region == Aws::Region::AWS_GLOBAL)) {
    m_allowRedirects = false;
  } else {
    m_allowRedirects = true;
  }
}

std::shared_ptr<Aws::Http::HttpResponse> CurlHttpClient::MakeRequest(
    const std::shared_ptr<Aws::Http::HttpRequest>& request,
    Aws::Utils::RateLimits::RateLimiterInterface* readLimiter,
    Aws::Utils::RateLimits::RateLimiterInterface* writeLimiter) const {
  auto start = util::time::now();
  Aws::Http::URI uri = request->GetUri();
  Aws::String url = uri.GetURIString();
  std::shared_ptr<Aws::Http::HttpResponse> response =
      Aws::MakeShared<Aws::Http::Standard::StandardHttpResponse>(CURL_HTTP_CLIENT_TAG,
                                                                 request);

  AWS_LOGSTREAM_TRACE(CURL_HTTP_CLIENT_TAG, "Making request to " << url);
  struct curl_slist* headers = NULL;

  if (writeLimiter != nullptr) {
    writeLimiter->ApplyAndPayForCost(request->GetSize());
  }

  Aws::StringStream headerStream;
  Aws::Http::HeaderValueCollection requestHeaders = request->GetHeaders();

  AWS_LOGSTREAM_TRACE(CURL_HTTP_CLIENT_TAG, "Including headers:");
  for (auto& requestHeader : requestHeaders) {
    headerStream.str("");
    headerStream << requestHeader.first << ": " << requestHeader.second;
    Aws::String headerString = headerStream.str();
    AWS_LOGSTREAM_TRACE(CURL_HTTP_CLIENT_TAG, headerString);
    headers = curl_slist_append(headers, headerString.c_str());
  }

  if (!request->HasHeader(Aws::Http::TRANSFER_ENCODING_HEADER)) {
    headers = curl_slist_append(headers, "transfer-encoding:");
  }

  if (!request->HasHeader(Aws::Http::CONTENT_LENGTH_HEADER)) {
    headers = curl_slist_append(headers, "content-length:");
  }

  if (!request->HasHeader(Aws::Http::CONTENT_TYPE_HEADER)) {
    headers = curl_slist_append(headers, "content-type:");
  }

  // Discard Expect header so as to avoid using multiple payloads to send a http request
  // (header + body)
  if (m_disableExpectHeader) {
    headers = curl_slist_append(headers, "Expect:");
  }

  CURL* connectionHandle = m_curlHandleContainer.AcquireCurlHandle(uri.GetAuthority());

  if (connectionHandle) {
    AWS_LOGSTREAM_DEBUG(CURL_HTTP_CLIENT_TAG,
                        "Obtained connection handle " << connectionHandle);

    if (headers) {
      curl_easy_setopt(connectionHandle, CURLOPT_HTTPHEADER, headers);
    }

    CurlWriteCallbackContext writeContext(this, request.get(), response.get(),
                                          readLimiter);
    CurlReadCallbackContext readContext(this, request.get(), writeLimiter);

    SetOptCodeForHttpMethod(connectionHandle, request);

    curl_easy_setopt(connectionHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(connectionHandle, CURLOPT_WRITEFUNCTION, WriteData);
    curl_easy_setopt(connectionHandle, CURLOPT_WRITEDATA, &writeContext);
    curl_easy_setopt(connectionHandle, CURLOPT_HEADERFUNCTION, WriteHeader);
    curl_easy_setopt(connectionHandle, CURLOPT_HEADERDATA, &writeContext);

    // we only want to override the default path if someone has explicitly told us to.
    if (!m_caPath.empty()) {
      curl_easy_setopt(connectionHandle, CURLOPT_CAPATH, m_caPath.c_str());
    }
    if (!m_caFile.empty()) {
      curl_easy_setopt(connectionHandle, CURLOPT_CAINFO, m_caFile.c_str());
    }

    // only set by android test builds because the emulator is missing a cert needed for
    // aws services
#ifdef TEST_CERT_PATH
    curl_easy_setopt(connectionHandle, CURLOPT_CAPATH, TEST_CERT_PATH);
#endif  // TEST_CERT_PATH

    if (m_verifySSL) {
      curl_easy_setopt(connectionHandle, CURLOPT_SSL_VERIFYPEER, 1L);
      curl_easy_setopt(connectionHandle, CURLOPT_SSL_VERIFYHOST, 2L);

#if LIBCURL_VERSION_MAJOR >= 7
#if LIBCURL_VERSION_MINOR >= 34
      curl_easy_setopt(connectionHandle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
#endif  // LIBCURL_VERSION_MINOR
#endif  // LIBCURL_VERSION_MAJOR
    } else {
      curl_easy_setopt(connectionHandle, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(connectionHandle, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    if (m_allowRedirects) {
      curl_easy_setopt(connectionHandle, CURLOPT_FOLLOWLOCATION, 1L);
    } else {
      curl_easy_setopt(connectionHandle, CURLOPT_FOLLOWLOCATION, 0L);
    }

#ifdef ENABLE_CURL_LOGGING
    curl_easy_setopt(connectionHandle, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(connectionHandle, CURLOPT_DEBUGFUNCTION, CurlDebugCallback);
#endif
    if (m_isUsingProxy) {
      Aws::StringStream ss;
      ss << m_proxyScheme << "://" << m_proxyHost;
      curl_easy_setopt(connectionHandle, CURLOPT_PROXY, ss.str().c_str());
      curl_easy_setopt(connectionHandle, CURLOPT_PROXYPORT, (long)m_proxyPort);
      if (!m_proxyUserName.empty() || !m_proxyPassword.empty()) {
        curl_easy_setopt(connectionHandle, CURLOPT_PROXYUSERNAME,
                         m_proxyUserName.c_str());
        curl_easy_setopt(connectionHandle, CURLOPT_PROXYPASSWORD,
                         m_proxyPassword.c_str());
      }
#ifdef CURL_HAS_TLS_PROXY
      if (!m_proxySSLCertPath.empty()) {
        curl_easy_setopt(connectionHandle, CURLOPT_PROXY_SSLCERT,
                         m_proxySSLCertPath.c_str());
        if (!m_proxySSLCertType.empty()) {
          curl_easy_setopt(connectionHandle, CURLOPT_PROXY_SSLCERTTYPE,
                           m_proxySSLCertType.c_str());
        }
      }
      if (!m_proxySSLKeyPath.empty()) {
        curl_easy_setopt(connectionHandle, CURLOPT_PROXY_SSLKEY,
                         m_proxySSLKeyPath.c_str());
        if (!m_proxySSLKeyType.empty()) {
          curl_easy_setopt(connectionHandle, CURLOPT_PROXY_SSLKEYTYPE,
                           m_proxySSLKeyType.c_str());
        }
        if (!m_proxyKeyPasswd.empty()) {
          curl_easy_setopt(connectionHandle, CURLOPT_PROXY_KEYPASSWD,
                           m_proxyKeyPasswd.c_str());
        }
      }
#endif  // CURL_HAS_TLS_PROXY
    } else {
      curl_easy_setopt(connectionHandle, CURLOPT_PROXY, "");
    }

    if (request->GetContentBody()) {
      curl_easy_setopt(connectionHandle, CURLOPT_READFUNCTION, ReadBody);
      curl_easy_setopt(connectionHandle, CURLOPT_READDATA, &readContext);
      curl_easy_setopt(connectionHandle, CURLOPT_SEEKFUNCTION, SeekBody);
      curl_easy_setopt(connectionHandle, CURLOPT_SEEKDATA, &readContext);
    }
    OverrideOptionsOnConnectionHandle(connectionHandle);
    Aws::Utils::DateTime startTransmissionTime = Aws::Utils::DateTime::Now();
    CURLcode curlResponseCode = curl_easy_perform(connectionHandle);
    bool shouldContinueRequest = ContinueRequest(*request);
    if (curlResponseCode != CURLE_OK && shouldContinueRequest) {
      response->SetClientErrorType(Aws::Client::CoreErrors::NETWORK_CONNECTION);
      Aws::StringStream ss;
      ss << "curlCode: " << curlResponseCode << ", "
         << curl_easy_strerror(curlResponseCode);
      response->SetClientErrorMessage(ss.str());
      AWS_LOGSTREAM_ERROR(CURL_HTTP_CLIENT_TAG,
                          "Curl returned error code "
                              << curlResponseCode << " - "
                              << curl_easy_strerror(curlResponseCode));
    } else if (!shouldContinueRequest) {
      response->SetClientErrorType(Aws::Client::CoreErrors::USER_CANCELLED);
      response->SetClientErrorMessage("Request cancelled by user's continuation handler");
    } else {
      long responseCode;
      curl_easy_getinfo(connectionHandle, CURLINFO_RESPONSE_CODE, &responseCode);
      response->SetResponseCode(static_cast<Aws::Http::HttpResponseCode>(responseCode));
      AWS_LOGSTREAM_DEBUG(CURL_HTTP_CLIENT_TAG,
                          "Returned http response code " << responseCode);

      char* contentType = nullptr;
      curl_easy_getinfo(connectionHandle, CURLINFO_CONTENT_TYPE, &contentType);
      if (contentType) {
        response->SetContentType(contentType);
        AWS_LOGSTREAM_DEBUG(CURL_HTTP_CLIENT_TAG,
                            "Returned content type " << contentType);
      }

      if (request->GetMethod() != Aws::Http::HttpMethod::HTTP_HEAD &&
          writeContext.m_client->IsRequestProcessingEnabled() &&
          response->HasHeader(Aws::Http::CONTENT_LENGTH_HEADER)) {
        const Aws::String& contentLength =
            response->GetHeader(Aws::Http::CONTENT_LENGTH_HEADER);
        int64_t numBytesResponseReceived = writeContext.m_numBytesResponseReceived;
        AWS_LOGSTREAM_TRACE(CURL_HTTP_CLIENT_TAG,
                            "Response content-length header: " << contentLength);
        AWS_LOGSTREAM_TRACE(CURL_HTTP_CLIENT_TAG,
                            "Response body length: " << numBytesResponseReceived);
        if (Aws::Utils::StringUtils::ConvertToInt64(contentLength.c_str()) !=
            numBytesResponseReceived) {
          response->SetClientErrorType(Aws::Client::CoreErrors::NETWORK_CONNECTION);
          response->SetClientErrorMessage(
              "Response body length doesn't match the content-length header.");
          AWS_LOGSTREAM_ERROR(
              CURL_HTTP_CLIENT_TAG,
              "Response body length doesn't match the content-length header.");
        }
      }

      AWS_LOGSTREAM_DEBUG(CURL_HTTP_CLIENT_TAG,
                          "Releasing curl handle " << connectionHandle);
    }

    auto entry = Buzz::logger::NewEntry("client_stats");
    entry.IntField("curl_code", curlResponseCode);

    double dns_time = 0;
    CURLcode ret = curl_easy_getinfo(connectionHandle, CURLINFO_NAMELOOKUP_TIME,
                                     &dns_time);  // DNS Resolve Latency, seconds.
    if (ret == CURLE_OK) {
      request->AddRequestMetric(
          Aws::Monitoring::GetHttpClientMetricNameByType(
              Aws::Monitoring::HttpClientMetricsType::DnsLatency),
          static_cast<int64_t>(dns_time * 1000));  // to milliseconds
      entry.IntField("dns_ms", dns_time * 1000);
    }

    double starttransfer_time = 0;
    ret = curl_easy_getinfo(connectionHandle, CURLINFO_STARTTRANSFER_TIME,
                            &starttransfer_time);  // Connect Latency
    if (ret == CURLE_OK) {
      request->AddRequestMetric(
          Aws::Monitoring::GetHttpClientMetricNameByType(
              Aws::Monitoring::HttpClientMetricsType::ConnectLatency),
          static_cast<int64_t>(starttransfer_time * 1000));
      entry.IntField("starttransfer_ms", starttransfer_time * 1000);
    }

    double appconnect_time = 0;
    ret = curl_easy_getinfo(connectionHandle, CURLINFO_APPCONNECT_TIME,
                            &appconnect_time);  // Ssl Latency
    if (ret == CURLE_OK) {
      request->AddRequestMetric(Aws::Monitoring::GetHttpClientMetricNameByType(
                                    Aws::Monitoring::HttpClientMetricsType::SslLatency),
                                static_cast<int64_t>(appconnect_time * 1000));
      entry.IntField("appconnect_ms", appconnect_time * 1000);
    }

    entry.IntField("total_ms", util::get_duration_ms(start, util::time::now()));
    entry.Log();

    const char* ip = nullptr;
    auto curlGetInfoResult =
        curl_easy_getinfo(connectionHandle, CURLINFO_PRIMARY_IP,
                          &ip);  // Get the IP address of the remote endpoint
    if (curlGetInfoResult == CURLE_OK && ip) {
      request->SetResolvedRemoteHost(ip);
    }
    if (curlResponseCode != CURLE_OK) {
      m_curlHandleContainer.DestroyCurlHandle(connectionHandle);
    } else {
      m_curlHandleContainer.ReleaseCurlHandle(uri.GetAuthority(), connectionHandle);
    }
    // go ahead and flush the response body stream
    response->GetResponseBody().flush();
    request->AddRequestMetric(
        Aws::Monitoring::GetHttpClientMetricNameByType(
            Aws::Monitoring::HttpClientMetricsType::RequestLatency),
        (Aws::Utils::DateTime::Now() - startTransmissionTime).count());
  }

  if (headers) {
    curl_slist_free_all(headers);
  }

  return response;
}

}  // namespace Http
}  // namespace Buzz