#pragma once

#include <string>

class HttpClient
{
public:
    explicit HttpClient(std::string endpointUrl);

    // JSON 文字列を POST 送信
    bool postJson(const std::string& jsonString) const;

private:
    std::string endpointUrl_;
};