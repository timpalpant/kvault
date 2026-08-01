#pragma once

#include "securebytes.h"

#include <QByteArray>
#include <optional>

namespace kvault::Crypto {

/// SHA-256 digest.
QByteArray sha256(const QByteArray &data);

/// SHA-1 digest (only for legacy RSA-OAEP-SHA1 EncStrings).
QByteArray sha1(const QByteArray &data);

/// HMAC-SHA256.
QByteArray hmacSha256(const SecureBytes &key, const QByteArray &data);

/**
 * PBKDF2 with HMAC-SHA256.
 * @param iterations must be >= 1.
 */
SecureBytes pbkdf2Sha256(const SecureBytes &password, const QByteArray &salt, int iterations, int outLen = 32);

/**
 * Argon2id.
 * @param memoryKiB memory cost in KiB (Bitwarden stores MiB, multiply by 1024).
 */
std::optional<SecureBytes>
argon2id(const SecureBytes &password, const QByteArray &salt, int iterations, int memoryKiB, int parallelism, int outLen = 32);

/**
 * HKDF-Expand (RFC 5869 section 2.3) with SHA-256, no extract step.
 * Bitwarden uses expand-only to stretch the 32-byte master key into a 64-byte
 * encryption + MAC key pair.
 */
SecureBytes hkdfExpandSha256(const SecureBytes &prk, const QByteArray &info, int outLen);

/// AES-256-CBC (or AES-128-CBC, chosen by key length) with PKCS#7 padding.
std::optional<SecureBytes> aesCbcDecrypt(const SecureBytes &key, const QByteArray &iv, const QByteArray &ciphertext);
std::optional<QByteArray> aesCbcEncrypt(const SecureBytes &key, const QByteArray &iv, const SecureBytes &plaintext);

/**
 * RSA-OAEP decryption using a PKCS#8 DER private key.
 * @param useSha256 selects the OAEP digest; Bitwarden's type 4 EncStrings use SHA-1.
 */
std::optional<SecureBytes> rsaOaepDecrypt(const SecureBytes &pkcs8PrivateKey, const QByteArray &ciphertext, bool useSha256);

} // namespace kvault::Crypto
