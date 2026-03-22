#include "Crypto.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <cstring>
#include <stdexcept>
#include <vector>

static constexpr int AES_KEY_LEN = 32;  // 256 bits
static constexpr int IV_LEN      = 16;  // 128-bit block size

void Crypto::derive_key(const std::string& passphrase, unsigned char* out_key) {
    SHA256(reinterpret_cast<const unsigned char*>(passphrase.data()),
           passphrase.size(), out_key);
}

std::string Crypto::base64_encode(const unsigned char* data, std::size_t len) {
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, static_cast<int>(len));
    BIO_flush(b64);

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}

std::string Crypto::base64_decode(const std::string& encoded) {
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    bmem = BIO_push(b64, bmem);
    BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);

    std::vector<unsigned char> buf(encoded.size());
    int decoded_len = BIO_read(bmem, buf.data(), static_cast<int>(buf.size()));
    BIO_free_all(bmem);

    if (decoded_len < 0)
        throw std::runtime_error("base64 decode failed");

    return std::string(buf.begin(), buf.begin() + decoded_len);
}

std::string Crypto::encrypt(const std::string& plaintext, const std::string& passphrase) {
    unsigned char key[AES_KEY_LEN];
    derive_key(passphrase, key);

    unsigned char iv[IV_LEN];
    if (RAND_bytes(iv, IV_LEN) != 1)
        throw std::runtime_error("RAND_bytes failed");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    std::vector<unsigned char> ciphertext(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
    int out_len = 0, final_len = 0;

    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len,
            reinterpret_cast<const unsigned char*>(plaintext.data()),
            static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptUpdate failed");
    }

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }

    EVP_CIPHER_CTX_free(ctx);

    int total = out_len + final_len;

    // Prepend IV to ciphertext before base64 encoding
    std::vector<unsigned char> iv_plus_ct(IV_LEN + total);
    std::memcpy(iv_plus_ct.data(), iv, IV_LEN);
    std::memcpy(iv_plus_ct.data() + IV_LEN, ciphertext.data(), total);

    return base64_encode(iv_plus_ct.data(), iv_plus_ct.size());
}

std::string Crypto::decrypt(const std::string& ciphertext_b64, const std::string& passphrase) {
    unsigned char key[AES_KEY_LEN];
    derive_key(passphrase, key);

    std::string raw = base64_decode(ciphertext_b64);
    if (raw.size() < static_cast<std::size_t>(IV_LEN))
        throw std::runtime_error("ciphertext too short");

    const unsigned char* iv = reinterpret_cast<const unsigned char*>(raw.data());
    const unsigned char* ct = reinterpret_cast<const unsigned char*>(raw.data()) + IV_LEN;
    int ct_len = static_cast<int>(raw.size()) - IV_LEN;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }

    std::vector<unsigned char> plaintext(ct_len + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
    int out_len = 0, final_len = 0;

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &out_len, ct, ct_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptUpdate failed");
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptFinal_ex failed");
    }

    EVP_CIPHER_CTX_free(ctx);

    return std::string(reinterpret_cast<char*>(plaintext.data()), out_len + final_len);
}
