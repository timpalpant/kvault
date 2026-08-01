#include "encstring.h"

#include "crypto.h"

#include <QLoggingCategory>
#include <QStringView>

Q_DECLARE_LOGGING_CATEGORY(KVAULT_CRYPTO)

namespace kvault {

namespace {

bool typeHasMac(EncString::Type type)
{
    switch (type) {
    case EncString::AesCbc128_HmacSha256_B64:
    case EncString::AesCbc256_HmacSha256_B64:
    case EncString::Rsa2048_OaepSha256_HmacSha256_B64:
    case EncString::Rsa2048_OaepSha1_HmacSha256_B64:
        return true;
    default:
        return false;
    }
}

bool isKnownType(int value)
{
    return value >= 0 && value <= 7;
}

/// Types whose payload is a single base64 blob rather than iv|ct[|mac].
bool isSinglePartType(EncString::Type type)
{
    return EncString::isRsaType(type) || type == EncString::CoseEncrypt0;
}

} // namespace

bool EncString::isRsaType(Type type)
{
    return type == Rsa2048_OaepSha256_B64 || type == Rsa2048_OaepSha1_B64 || type == Rsa2048_OaepSha256_HmacSha256_B64
           || type == Rsa2048_OaepSha1_HmacSha256_B64;
}

bool EncString::isSupportedType(Type type)
{
    // Everything except the COSE/XChaCha20 format, which KVault does not
    // implement. It is parsed only so it can be reported clearly.
    return type != Invalid && type != CoseEncrypt0;
}

EncString EncString::parse(const QString &encoded)
{
    if (encoded.isEmpty()) {
        return {};
    }

    Type type = AesCbc256_B64;
    QStringView payload(encoded);

    // Base64 never contains '.', so the first one is the type separator.
    const qsizetype dot = encoded.indexOf(u'.');
    if (dot > 0) {
        bool ok = false;
        const int parsed = QStringView(encoded).left(dot).toInt(&ok);
        if (!ok || !isKnownType(parsed)) {
            return {};
        }
        type = Type(parsed);
        payload = payload.mid(dot + 1);
    }

    const QList<QStringView> parts = payload.split(u'|');
    EncString result;
    result.m_type = type;

    if (isSinglePartType(type)) {
        if (parts.isEmpty() || parts.at(0).isEmpty()) {
            return {};
        }
        result.m_ciphertext = QByteArray::fromBase64(parts.at(0).toLatin1());
        if (parts.size() > 1) {
            result.m_mac = QByteArray::fromBase64(parts.at(1).toLatin1());
        }
    } else {
        if (parts.size() < 2) {
            return {};
        }
        result.m_iv = QByteArray::fromBase64(parts.at(0).toLatin1());
        result.m_ciphertext = QByteArray::fromBase64(parts.at(1).toLatin1());
        if (parts.size() > 2) {
            result.m_mac = QByteArray::fromBase64(parts.at(2).toLatin1());
        }
        if (result.m_iv.size() != 16) {
            return {};
        }
    }

    if (result.m_ciphertext.isEmpty()) {
        return {};
    }
    if (typeHasMac(type) && result.m_mac.isEmpty()) {
        return {};
    }
    return result;
}

QString EncString::toString() const
{
    if (isNull()) {
        return {};
    }
    QString out = QString::number(int(m_type)) + u'.';
    if (!isRsaType(m_type)) {
        out += QString::fromLatin1(m_iv.toBase64()) + u'|';
    }
    out += QString::fromLatin1(m_ciphertext.toBase64());
    if (!m_mac.isEmpty()) {
        out += u'|' + QString::fromLatin1(m_mac.toBase64());
    }
    return out;
}

std::optional<SecureBytes> EncString::decrypt(const SymmetricKey &key) const
{
    if (m_type == CoseEncrypt0) {
        qCWarning(KVAULT_CRYPTO) << "Value uses the COSE/XChaCha20 format, which KVault cannot decrypt";
        return std::nullopt;
    }
    if (isNull() || !key.isValid() || isRsaType(m_type)) {
        return std::nullopt;
    }

    if (typeHasMac(m_type)) {
        if (!key.hasMacKey()) {
            return std::nullopt;
        }
        const QByteArray expected = Crypto::hmacSha256(key.macKey(), m_iv + m_ciphertext);
        if (expected.isEmpty() || !constantTimeEquals(expected, m_mac)) {
            qCWarning(KVAULT_CRYPTO) << "EncString MAC verification failed";
            return std::nullopt;
        }
    }

    return Crypto::aesCbcDecrypt(key.encKey(), m_iv, m_ciphertext);
}

std::optional<QString> EncString::decryptString(const SymmetricKey &key) const
{
    auto plain = decrypt(key);
    if (!plain) {
        return std::nullopt;
    }
    return plain->toString();
}

std::optional<SecureBytes> EncString::decryptRsa(const SecureBytes &pkcs8PrivateKey) const
{
    if (isNull() || !isRsaType(m_type)) {
        return std::nullopt;
    }
    const bool sha256 = (m_type == Rsa2048_OaepSha256_B64 || m_type == Rsa2048_OaepSha256_HmacSha256_B64);
    return Crypto::rsaOaepDecrypt(pkcs8PrivateKey, m_ciphertext, sha256);
}

EncString EncString::encrypt(const SecureBytes &plaintext, const SymmetricKey &key)
{
    if (!key.isValid()) {
        return {};
    }
    const SecureBytes ivBytes = SecureBytes::random(16);
    const QByteArray iv = ivBytes.toByteArray();

    auto ciphertext = Crypto::aesCbcEncrypt(key.encKey(), iv, plaintext);
    if (!ciphertext) {
        return {};
    }

    EncString result;
    result.m_iv = iv;
    result.m_ciphertext = *ciphertext;
    if (key.hasMacKey()) {
        result.m_type = AesCbc256_HmacSha256_B64;
        result.m_mac = Crypto::hmacSha256(key.macKey(), iv + *ciphertext);
        if (result.m_mac.isEmpty()) {
            return {};
        }
    } else {
        result.m_type = AesCbc256_B64;
    }
    return result;
}

EncString EncString::encryptString(const QString &plaintext, const SymmetricKey &key)
{
    return encrypt(SecureBytes::fromString(plaintext), key);
}

EncString EncString::fromPackedBuffer(const QByteArray &buffer)
{
    if (buffer.size() < 2) {
        return {};
    }
    const int rawType = uint8_t(buffer.at(0));
    if (!isKnownType(rawType)) {
        return {};
    }
    const Type type = Type(rawType);
    if (isSinglePartType(type) || !isSupportedType(type)) {
        // Not a format Bitwarden produces for blobs, or one we cannot read.
        return {};
    }

    const qsizetype macLen = typeHasMac(type) ? 32 : 0;
    const qsizetype headerLen = 1 + 16 + macLen;
    if (buffer.size() <= headerLen) {
        return {};
    }

    EncString result;
    result.m_type = type;
    result.m_iv = buffer.mid(1, 16);
    if (macLen) {
        result.m_mac = buffer.mid(17, macLen);
    }
    result.m_ciphertext = buffer.mid(headerLen);
    return result;
}

QByteArray EncString::toPackedBuffer() const
{
    if (isNull() || isSinglePartType(m_type) || !isSupportedType(m_type)) {
        return {};
    }
    QByteArray out;
    out.append(char(m_type));
    out.append(m_iv);
    out.append(m_mac);
    out.append(m_ciphertext);
    return out;
}

} // namespace kvault
