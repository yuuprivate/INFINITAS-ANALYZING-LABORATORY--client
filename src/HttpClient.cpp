#include "HttpClient.h"
#include "Logger.h"
#include <windows.h>
#include <wininet.h>
#include <vector>

#pragma comment(lib, "wininet.lib")

HttpClient::HttpClient(std::string endpointUrl)
    : endpointUrl_(std::move(endpointUrl))
{
}

bool HttpClient::postJson(const std::string& jsonString) const
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

    // ★ 修正点 1: URL から抽出したポート番号を使用する（無指定の場合はスキームに応じてデフォルト値）
    INTERNET_PORT port = urlComp.nPort;
    if (port == 0)
    {
        port = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    }

    HINTERNET hInternet = InternetOpenA("InfinitasTracker/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet)
    {
        LOG_ERROR("HTTP POST エラー: InternetOpen に失敗しました。 Error: " + std::to_string(GetLastError()));
        return false;
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_FLAG_SECURE : 0;

    // ★ 修正点 2: 取得した port (8000) を渡す
    HINTERNET hConnect = InternetConnectA(hInternet, hostName, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect)
    {
        LOG_ERROR("HTTP POST エラー: サーバーとの接続確立に失敗しました (" + std::string(hostName) + ":" + std::to_string(port) + ") Error: " + std::to_string(GetLastError()));
        InternetCloseHandle(hInternet);
        return false;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlPath, NULL, NULL, NULL, flags, 0);
    if (!hRequest)
    {
        LOG_ERROR("HTTP POST エラー: リクエストの初期化に失敗しました。 Error: " + std::to_string(GetLastError()));
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    std::string headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(
        hRequest,
        headers.c_str(),
        static_cast<DWORD>(headers.length()),
        const_cast<char*>(jsonString.data()),
        static_cast<DWORD>(jsonString.length())
    );

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
        // ★ 修正点 3: 失敗時の Win32 エラーコードを出力
        LOG_ERROR("HTTP POST 送信リクエストの実行に失敗しました。 Win32 Error: " + std::to_string(GetLastError()));
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return isSuccess;
}