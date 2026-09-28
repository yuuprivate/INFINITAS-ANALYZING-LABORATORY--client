#pragma once
#include <string>

namespace Utils
{
    // UUID v4 形式の nonce (一意な文字列) を生成する
    std::string generateNonce();
}