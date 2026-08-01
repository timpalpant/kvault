#include "crypto.h"

#include <argon2.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pkcs12.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <QLoggingCategory>
#include <memory>

Q_LOGGING_CATEGORY(KVAULT_CRYPTO, "kvault.crypto")

namespace kvault::Crypto {

namespace {

struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX *p) const { EVP_MD_CTX_free(p); }
};
struct EvpCipherCtxDeleter {
    void operator()(EVP_CIPHER_CTX *p) const { EVP_CIPHER_CTX_free(p); }
};
struct EvpPkeyDeleter {
    void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
struct EvpPkeyCtxDeleter {
    void operator()(EVP_PKEY_CTX *p) const { EVP_PKEY_CTX_free(p); }
};

QByteArray digest(const EVP_MD *md, const QByteArray &data)
{
    std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter> ctx(EVP_MD_CTX_new());
    if (!ctx) {
        return {};
    }
    QByteArray out(EVP_MD_get_size(md), Qt::Uninitialized);
    unsigned int len = 0;
    if (EVP_DigestInit_ex(ctx.get(), md, nullptr) != 1 || EVP_DigestUpdate(ctx.get(), data.constData(), size_t(data.size())) != 1
        || EVP_DigestFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(out.data()), &len) != 1) {
        return {};
    }
    out.resize(qsizetype(len));
    return out;
}

} // namespace

QByteArray sha256(const QByteArray &data)
{
    return digest(EVP_sha256(), data);
}

QByteArray sha1(const QByteArray &data)
{
    return digest(EVP_sha1(), data);
}

QByteArray hmacSha256(const SecureBytes &key, const QByteArray &data)
{
    QByteArray out(EVP_MAX_MD_SIZE, Qt::Uninitialized);
    unsigned int len = 0;
    const unsigned char *result = HMAC(EVP_sha256(),
                                       key.constData(),
                                       int(key.size()),
                                       reinterpret_cast<const unsigned char *>(data.constData()),
                                       size_t(data.size()),
                                       reinterpret_cast<unsigned char *>(out.data()),
                                       &len);
    if (!result) {
        return {};
    }
    out.resize(qsizetype(len));
    return out;
}

SecureBytes pbkdf2Sha256(const SecureBytes &password, const QByteArray &salt, int iterations, int outLen)
{
    if (iterations < 1 || outLen < 1) {
        return {};
    }
    SecureBytes out(outLen);
    const int rc = PKCS5_PBKDF2_HMAC(reinterpret_cast<const char *>(password.constData()),
                                     int(password.size()),
                                     reinterpret_cast<const unsigned char *>(salt.constData()),
                                     int(salt.size()),
                                     iterations,
                                     EVP_sha256(),
                                     outLen,
                                     out.data());
    if (rc != 1) {
        qCWarning(KVAULT_CRYPTO) << "PBKDF2 failed";
        return {};
    }
    return out;
}

std::optional<SecureBytes> argon2id(const SecureBytes &password, const QByteArray &salt, int iterations, int memoryKiB, int parallelism, int outLen)
{
    if (iterations < 1 || parallelism < 1 || outLen < 1 || salt.size() < 8) {
        return std::nullopt;
    }
    // libargon2 rejects memory below 8 blocks per lane.
    const uint32_t minMemory = 8u * uint32_t(parallelism);
    const uint32_t memory = qMax(uint32_t(memoryKiB), minMemory);

    SecureBytes out(outLen);
    const int rc = argon2id_hash_raw(uint32_t(iterations),
                                     memory,
                                     uint32_t(parallelism),
                                     password.constData(),
                                     size_t(password.size()),
                                     salt.constData(),
                                     size_t(salt.size()),
                                     out.data(),
                                     size_t(outLen));
    if (rc != ARGON2_OK) {
        qCWarning(KVAULT_CRYPTO) << "Argon2id failed:" << argon2_error_message(rc);
        return std::nullopt;
    }
    return out;
}

SecureBytes hkdfExpandSha256(const SecureBytes &prk, const QByteArray &info, int outLen)
{
    constexpr int hashLen = 32;
    if (outLen < 1 || outLen > 255 * hashLen) {
        return {};
    }
    const int blocks = (outLen + hashLen - 1) / hashLen;

    SecureBytes out;
    QByteArray previous;
    for (int i = 1; i <= blocks; ++i) {
        QByteArray input = previous;
        input.append(info);
        input.append(char(i));
        previous = hmacSha256(prk, input);
        if (previous.size() != hashLen) {
            return {};
        }
        out.append(previous.constData(), previous.size());
    }
    secureZero(previous.data(), size_t(previous.size()));
    return out.mid(0, outLen);
}

std::optional<SecureBytes> aesCbcDecrypt(const SecureBytes &key, const QByteArray &iv, const QByteArray &ciphertext)
{
    const EVP_CIPHER *cipher = nullptr;
    if (key.size() == 32) {
        cipher = EVP_aes_256_cbc();
    } else if (key.size() == 16) {
        cipher = EVP_aes_128_cbc();
    } else {
        return std::nullopt;
    }
    if (iv.size() != 16 || ciphertext.isEmpty() || ciphertext.size() % 16 != 0) {
        return std::nullopt;
    }

    std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter> ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        return std::nullopt;
    }
    if (EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, key.constData(), reinterpret_cast<const unsigned char *>(iv.constData())) != 1) {
        return std::nullopt;
    }

    SecureBytes out(ciphertext.size() + EVP_CIPHER_block_size(cipher));
    int len = 0;
    int total = 0;
    if (EVP_DecryptUpdate(ctx.get(), out.data(), &len, reinterpret_cast<const unsigned char *>(ciphertext.constData()), int(ciphertext.size()))
        != 1) {
        return std::nullopt;
    }
    total = len;
    if (EVP_DecryptFinal_ex(ctx.get(), out.data() + total, &len) != 1) {
        // Bad padding: wrong key, or corrupted data.
        return std::nullopt;
    }
    total += len;
    return out.mid(0, total);
}

std::optional<QByteArray> aesCbcEncrypt(const SecureBytes &key, const QByteArray &iv, const SecureBytes &plaintext)
{
    const EVP_CIPHER *cipher = nullptr;
    if (key.size() == 32) {
        cipher = EVP_aes_256_cbc();
    } else if (key.size() == 16) {
        cipher = EVP_aes_128_cbc();
    } else {
        return std::nullopt;
    }
    if (iv.size() != 16) {
        return std::nullopt;
    }

    std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter> ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        return std::nullopt;
    }
    if (EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, key.constData(), reinterpret_cast<const unsigned char *>(iv.constData())) != 1) {
        return std::nullopt;
    }

    QByteArray out(plaintext.size() + EVP_CIPHER_block_size(cipher), Qt::Uninitialized);
    int len = 0;
    int total = 0;
    if (EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(out.data()), &len, plaintext.constData(), int(plaintext.size())) != 1) {
        return std::nullopt;
    }
    total = len;
    if (EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(out.data()) + total, &len) != 1) {
        return std::nullopt;
    }
    total += len;
    out.resize(total);
    return out;
}

std::optional<SecureBytes> rsaOaepDecrypt(const SecureBytes &pkcs8PrivateKey, const QByteArray &ciphertext, bool useSha256)
{
    const unsigned char *der = pkcs8PrivateKey.constData();
    PKCS8_PRIV_KEY_INFO *p8 = d2i_PKCS8_PRIV_KEY_INFO(nullptr, &der, long(pkcs8PrivateKey.size()));
    if (!p8) {
        qCWarning(KVAULT_CRYPTO) << "Could not parse PKCS#8 private key";
        return std::nullopt;
    }
    std::unique_ptr<EVP_PKEY, EvpPkeyDeleter> pkey(EVP_PKCS82PKEY(p8));
    PKCS8_PRIV_KEY_INFO_free(p8);
    if (!pkey) {
        return std::nullopt;
    }

    std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter> ctx(EVP_PKEY_CTX_new(pkey.get(), nullptr));
    if (!ctx || EVP_PKEY_decrypt_init(ctx.get()) != 1) {
        return std::nullopt;
    }
    const EVP_MD *md = useSha256 ? EVP_sha256() : EVP_sha1();
    if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) != 1 || EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), md) != 1
        || EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), md) != 1) {
        return std::nullopt;
    }

    const auto *in = reinterpret_cast<const unsigned char *>(ciphertext.constData());
    size_t outLen = 0;
    if (EVP_PKEY_decrypt(ctx.get(), nullptr, &outLen, in, size_t(ciphertext.size())) != 1) {
        return std::nullopt;
    }
    SecureBytes out{qsizetype(outLen)};
    if (EVP_PKEY_decrypt(ctx.get(), out.data(), &outLen, in, size_t(ciphertext.size())) != 1) {
        return std::nullopt;
    }
    return out.mid(0, qsizetype(outLen));
}

} // namespace kvault::Crypto
