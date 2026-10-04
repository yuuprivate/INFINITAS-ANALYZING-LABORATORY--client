#pragma once

#include <string>
#include <windows.h>
#include <wininet.h>

class HttpClient
{
public:
    explicit HttpClient(std::string inputUrl, bool isBaseUrl = false);

    bool postJson(const std::string &jsonString) const;

    bool postJsonWithFallback(const std::string &jsonString);

    std::string fetchLatestUrlFromGist(const std::string &gistRawUrl) const;

private:
    std::string endpointUrl_;
    std::string gistFallbackUrl_;
    bool isBaseUrl_;

    std::string fetchNonce(const std::string &hostName, INTERNET_PORT port, bool isHttps) const;
};