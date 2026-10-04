#include "HttpClient.h"
#include "Logger.h"
#include <windows.h>
#include <wininet.h>
#include <vector>

#pragma comment(lib, "wininet.lib")

#include <fstream>
#include <sstream>
#include <string>

// config.txt 内の特定キーの値を書き換える関数
bool updateConfigApiEndpoint(const std::string &configPath, const std::string &newUrl)
{
    std::ifstream inFile(configPath);
    if (!inFile.is_open())
    {
        return false;
    }

    std::stringstream buffer;
    std::string line;
    bool updated = false;

    while (std::getline(inFile, line))
    {
        // "API_ENDPOINT=" で始まる行を探す
        if (line.rfind("API_ENDPOINT=", 0) == 0)
        {
            buffer << "API_ENDPOINT=" << newUrl << "\n";
            updated = true;
        }
        else
        {
            buffer << line << "\n";
        }
    }
    inFile.close();

    // もし API_ENDPOINT の行がファイル内に存在しなかった場合は追記する
    if (!updated)
    {
        buffer << "API_ENDPOINT=" << newUrl << "\n";
    }

    std::ofstream outFile(configPath, std::ios::trunc);
    if (!outFile.is_open())
    {
        return false;
    }

    outFile << buffer.str();
    return true;
}

HttpClient::HttpClient(std::string inputUrl, bool isBaseUrl) : isBaseUrl_(isBaseUrl)
{
    auto trim = [](std::string &s)
    {
        size_t first = s.find_first_not_of(" \n\r\t");
        if (first == std::string::npos)
        {
            s.clear();
            return;
        }
        size_t last = s.find_last_not_of(" \n\r\t");
        s = s.substr(first, (last - first + 1));
    };

    trim(inputUrl);

    if (inputUrl.find("gist.github") != std::string::npos)
    {
        gistFallbackUrl_ = inputUrl;

        LOG_INFO("GitHub Gist から最新のサーバーURLを取得しています: " + inputUrl);
        std::string resolvedUrl = fetchLatestUrlFromGist(inputUrl);
        trim(resolvedUrl); // 取得したURLも念のためトリム

        if (!resolvedUrl.empty())
        {
            endpointUrl_ = resolvedUrl;
            LOG_INFO("Gist から取得した URL を適用しました: " + endpointUrl_);
        }
        else
        {
            LOG_ERROR("Gist からのURL取得に失敗しました。");
            endpointUrl_ = inputUrl;
        }
    }
    else
    {
        endpointUrl_ = inputUrl;
        gistFallbackUrl_ = "https://gist.githubusercontent.com/yuuprivate/adfc3f98c80d2fda0653bf8c85a1feea/raw/url.txt";
    }
}

std::string HttpClient::fetchLatestUrlFromGist(const std::string &gistRawUrl) const
{
    URL_COMPONENTSA urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {0};
    char urlPath[1024] = {0};

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);

    if (!InternetCrackUrlA(gistRawUrl.c_str(), static_cast<DWORD>(gistRawUrl.length()), 0, &urlComp))
    {
        LOG_ERROR("Gist URL Crack failed: " + gistRawUrl);
        return "";
    }
    LOG_INFO("Parsed Hostname: " + std::string(hostName) + " | Path: " + std::string(urlPath));

    INTERNET_PORT port = urlComp.nPort;
    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    if (port == 0)
    {
        port = isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    }

    HINTERNET hInternet = InternetOpenA("InfinitasTracker/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
    {
        LOG_ERROR("InternetOpenA failed in Gist fetch: " + std::to_string(GetLastError()));
        return "";
    }

    HINTERNET hConnect = InternetConnectA(hInternet, hostName, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect)
    {
        LOG_ERROR("InternetConnectA failed in Gist fetch: " + std::to_string(GetLastError()) + " (Host: " + std::string(hostName) + ")");
        InternetCloseHandle(hInternet);
        return "";
    }

    DWORD flags = isHttps ? INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD : INTERNET_FLAG_RELOAD;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", urlPath, NULL, NULL, NULL, flags, 0);
    if (!hRequest)
    {
        LOG_ERROR("HttpOpenRequestA failed in Gist fetch: " + std::to_string(GetLastError()));
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string fetchedUrl = "";
    if (HttpSendRequestA(hRequest, NULL, 0, NULL, 0))
    {
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL);

        if (statusCode == 200)
        {
            char buffer[512] = {0};
            DWORD bytesRead = 0;
            std::string responseBody = "";

            while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                responseBody += buffer;
            }

            size_t first = responseBody.find_first_not_of(" \n\r\t");
            if (first != std::string::npos)
            {
                size_t last = responseBody.find_last_not_of(" \n\r\t");
                fetchedUrl = responseBody.substr(first, (last - first + 1));
            }
        }
        else
        {
            LOG_ERROR("Gist HTTP Status Code is not 200: " + std::to_string(statusCode));
        }
    }
    else
    {
        LOG_ERROR("HttpSendRequestA failed in Gist fetch: " + std::to_string(GetLastError()));
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return fetchedUrl;
}

std::string HttpClient::fetchNonce(const std::string &hostName, INTERNET_PORT port, bool isHttps) const
{
    HINTERNET hInternet = InternetOpenA("InfinitasTracker/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
    {
        LOG_ERROR("InternetOpenA failed: " + std::to_string(GetLastError()));
        return "";
    }

    HINTERNET hConnect = InternetConnectA(hInternet, hostName.c_str(), port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect)
    {
        LOG_ERROR("InternetConnectA failed: " + std::to_string(GetLastError()));
        InternetCloseHandle(hInternet);
        return "";
    }

    DWORD flags = isHttps ? INTERNET_FLAG_SECURE : 0;
    const char *noncePath = "/api/v1/auth/nonce";

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", noncePath, NULL, NULL, NULL, flags, 0);
    if (!hRequest)
    {
        LOG_ERROR("HttpOpenRequestA failed: " + std::to_string(GetLastError()));
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string nonceStr = "";
    if (HttpSendRequestA(hRequest, NULL, 0, NULL, 0))
    {
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL);

        if (statusCode == 200)
        {
            char buffer[512] = {0};
            DWORD bytesRead = 0;
            std::string responseBody = "";

            while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                responseBody += buffer;
            }

            size_t keyPos = responseBody.find("\"nonce\"");
            if (keyPos != std::string::npos)
            {
                size_t startQuote = responseBody.find("\"", keyPos + 7);
                if (startQuote != std::string::npos)
                {
                    size_t endQuote = responseBody.find("\"", startQuote + 1);
                    if (endQuote != std::string::npos)
                    {
                        nonceStr = responseBody.substr(startQuote + 1, endQuote - startQuote - 1);
                    }
                }
            }
        }
        else
        {
            LOG_ERROR("HTTP Status Code is not 200: " + std::to_string(statusCode));
        }
    }
    else
    {
        LOG_ERROR("HttpSendRequestA failed: " + std::to_string(GetLastError()));
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return nonceStr;
}

bool HttpClient::postJson(const std::string &jsonString) const
{
    URL_COMPONENTSA urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {0};
    char urlPath[1024] = {0};

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);

    if (!InternetCrackUrlA(endpointUrl_.c_str(), static_cast<DWORD>(endpointUrl_.length()), 0, &urlComp))
    {
        LOG_ERROR("HTTP POST エラー: URL のパースに失敗しました (" + endpointUrl_ + ")");
        return false;
    }

    INTERNET_PORT port = urlComp.nPort;
    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    if (port == 0)
    {
        port = isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    }

    // 1. まず Nonce を取得
    std::string nonce = fetchNonce(hostName, port, isHttps);
    if (nonce.empty())
    {
        LOG_ERROR("HTTP POST エラー: ワンタイム Nonce の取得に失敗したため、リクエストをキャンセルしました。");
        return false;
    }

    LOG_INFO("Nonce 取得成功: " + nonce);

    HINTERNET hInternet = InternetOpenA("InfinitasTracker/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
    {
        LOG_ERROR("HTTP POST エラー: InternetOpen に失敗しました。");
        return false;
    }

    DWORD flags = isHttps ? INTERNET_FLAG_SECURE : 0;

    HINTERNET hConnect = InternetConnectA(hInternet, hostName, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect)
    {
        LOG_ERROR("HTTP POST エラー: サーバーとの接続確立に失敗しました (" + std::string(hostName) + ":" + std::to_string(port) + ")");
        InternetCloseHandle(hInternet);
        return false;
    }

    // アップロード用のエンドポイントパスを固定で指定
    const char *uploadPath = "/api/v1/receiver/upload";

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", uploadPath, NULL, NULL, NULL, flags, 0);
    if (!hRequest)
    {
        LOG_ERROR("HTTP POST エラー: リクエストの初期化に失敗しました。");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    // ヘッダーに Content-Type と X-Nonce を付与
    std::string headers = "Content-Type: application/json\r\nX-Nonce: " + nonce + "\r\n";

    BOOL sent = HttpSendRequestA(
        hRequest,
        headers.c_str(),
        static_cast<DWORD>(headers.length()),
        const_cast<char *>(jsonString.data()),
        static_cast<DWORD>(jsonString.length()));

    bool isSuccess = false;
    if (sent)
    {
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL);

        if (statusCode >= 200 && statusCode < 300)
        {
            LOG_INFO("HTTP POST 成功 [Status: " + std::to_string(statusCode) + "]");
            isSuccess = true;
        }
        else
        {
            LOG_ERROR("HTTP POST エラー [Status: " + std::to_string(statusCode) + "]");
        }
    }
    else
    {
        LOG_ERROR("HTTP POST 送信リクエストの実行に失敗しました。 Win32 Error: " + std::to_string(GetLastError()));
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return isSuccess;
}

bool HttpClient::postJsonWithFallback(const std::string &jsonString)
{
    if (postJson(jsonString))
    {
        return true;
    }

    if (!gistFallbackUrl_.empty())
    {
        LOG_WARN("サーバーへの送信に失敗しました。Gistから最新のURLを再取得して再送を試みます...");

        std::string latestUrl = fetchLatestUrlFromGist(gistFallbackUrl_);

        if (!latestUrl.empty() && latestUrl != endpointUrl_)
        {
            // (省略: trim処理)

            endpointUrl_ = latestUrl;
            LOG_INFO("新しいエンドポイントに更新しました: " + endpointUrl_);

            bool retrySuccess = postJson(jsonString);
            if (retrySuccess)
            {
                // ★ ここで config.txt の API_ENDPOINT を書き換える！
                if (updateConfigApiEndpoint("config.txt", endpointUrl_))
                {
                    LOG_INFO("config.txt の API_ENDPOINT を新しいURLに自動更新しました。");
                }
                else
                {
                    LOG_ERROR("config.txt の自動更新に失敗しました。");
                }
                return true;
            }
        }
    }

    return false;
}