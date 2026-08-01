#pragma once

#include "securebytes.h"

#include <QJsonObject>
#include <QString>
#include <optional>

namespace kvault {

enum class KdfType {
    Pbkdf2Sha256 = 0,
    Argon2id = 1,
};

/// The per-account KDF settings the server reports from /accounts/prelogin.
struct KdfConfig {
    KdfType type = KdfType::Pbkdf2Sha256;
    int iterations = 600000;
    int memoryMiB = 64;  ///< Argon2id only
    int parallelism = 4; ///< Argon2id only

    static KdfConfig fromJson(const QJsonObject &json);
    QJsonObject toJson() const;
    bool isValid() const;
};

namespace AccountCrypto {

/**
 * Derive the 32-byte master key from the master password.
 *
 * PBKDF2 uses the lowercased, trimmed email as the salt; Argon2id uses the
 * SHA-256 of it.
 */
std::optional<SecureBytes> deriveMasterKey(const QString &email, const SecureBytes &password, const KdfConfig &kdf);

enum class HashPurpose {
    ServerAuthorization = 1, ///< what gets sent as the "master password hash"
    LocalAuthorization = 2,  ///< used to check the password without a server
};

/// Base64 of PBKDF2(masterKey, salt = password, iterations = purpose).
QString hashMasterPassword(const SecureBytes &masterKey, const SecureBytes &password, HashPurpose purpose);

/// Normalise an email the way the server does before using it as a salt.
QString normaliseEmail(const QString &email);

} // namespace AccountCrypto

} // namespace kvault
