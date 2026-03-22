#pragma once

#include <string>

class Crypto {
public:
    static std::string encrypt(const std::string& plaintext, const std::string& passphrase);
    static std::string decrypt(const std::string& ciphertext_b64, const std::string& passphrase);

private:
    static void derive_key(const std::string& passphrase, unsigned char* out_key);
    static std::string base64_encode(const unsigned char* data, std::size_t len);
    static std::string base64_decode(const std::string& encoded);
};
