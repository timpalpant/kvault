#include "kdf.h"

#include "crypto.h"

#include <QJsonValue>

namespace kvault {

KdfConfig KdfConfig::fromJson(const QJsonObject &json)
{
    KdfConfig config;
    // The server is inconsistent about capitalisation between the prelogin and
    // token endpoints, so accept both spellings.
    const auto value = [&json](const char *lower, const char *upper) -> QJsonValue {
        const QJsonValue v = json.value(QLatin1String(lower));
        return v.isUndefined() || v.isNull() ? json.value(QLatin1String(upper)) : v;
    };

    config.type = KdfType(value("kdf", "Kdf").toInt(0));
    config.iterations = value("kdfIterations", "KdfIterations").toInt(600000);

    const QJsonValue memory = value("kdfMemory", "KdfMemory");
    if (memory.isDouble()) {
        config.memoryMiB = memory.toInt();
    }
    const QJsonValue parallelism = value("kdfParallelism", "KdfParallelism");
    if (parallelism.isDouble()) {
        config.parallelism = parallelism.toInt();
    }
    return config;
}

QJsonObject KdfConfig::toJson() const
{
    QJsonObject json{
        {QStringLiteral("kdf"), int(type)},
        {QStringLiteral("kdfIterations"), iterations},
    };
    if (type == KdfType::Argon2id) {
        json.insert(QStringLiteral("kdfMemory"), memoryMiB);
        json.insert(QStringLiteral("kdfParallelism"), parallelism);
    }
    return json;
}

bool KdfConfig::isValid() const
{
    if (iterations < 1) {
        return false;
    }
    if (type == KdfType::Argon2id) {
        return memoryMiB >= 1 && parallelism >= 1;
    }
    return type == KdfType::Pbkdf2Sha256;
}

namespace AccountCrypto {

QString normaliseEmail(const QString &email)
{
    return email.trimmed().toLower();
}

std::optional<SecureBytes> deriveMasterKey(const QString &email, const SecureBytes &password, const KdfConfig &kdf)
{
    if (!kdf.isValid() || password.isEmpty()) {
        return std::nullopt;
    }
    const QByteArray salt = normaliseEmail(email).toUtf8();
    if (salt.isEmpty()) {
        return std::nullopt;
    }

    switch (kdf.type) {
    case KdfType::Pbkdf2Sha256: {
        SecureBytes key = Crypto::pbkdf2Sha256(password, salt, kdf.iterations, 32);
        if (key.isEmpty()) {
            return std::nullopt;
        }
        return key;
    }
    case KdfType::Argon2id:
        // Argon2id hashes the email first so the salt is always 32 bytes.
        return Crypto::argon2id(password, Crypto::sha256(salt), kdf.iterations, kdf.memoryMiB * 1024, kdf.parallelism, 32);
    }
    return std::nullopt;
}

QString hashMasterPassword(const SecureBytes &masterKey, const SecureBytes &password, HashPurpose purpose)
{
    if (masterKey.size() != 32 || password.isEmpty()) {
        return {};
    }
    const SecureBytes hash = Crypto::pbkdf2Sha256(masterKey, password.toByteArray(), int(purpose), 32);
    if (hash.isEmpty()) {
        return {};
    }
    return QString::fromLatin1(hash.toByteArray().toBase64());
}

} // namespace AccountCrypto

} // namespace kvault
