#pragma once

#include "symmetrickey.h"

#include <QString>
#include <optional>

namespace kvault {

/**
 * Bitwarden's "EncString": a self-describing ciphertext, serialised as
 *
 *     <type>.<base64 iv>|<base64 ciphertext>[|<base64 mac>]
 *
 * for the AES variants, and
 *
 *     <type>.<base64 ciphertext>[|<base64 mac>]
 *
 * for the RSA variants. A leading type marker may be absent in very old data,
 * in which case type 0 is assumed.
 */
class EncString
{
public:
    enum Type {
        AesCbc256_B64 = 0,
        AesCbc128_HmacSha256_B64 = 1,
        AesCbc256_HmacSha256_B64 = 2,
        Rsa2048_OaepSha256_B64 = 3,
        Rsa2048_OaepSha1_B64 = 4,
        Rsa2048_OaepSha256_HmacSha256_B64 = 5,
        Rsa2048_OaepSha1_HmacSha256_B64 = 6,
        /**
         * XChaCha20-Poly1305 in a COSE envelope, from Bitwarden's newer "v2"
         * encryption. Recognised so it can be reported precisely, but not
         * implemented; KVault asks the server for the classic format instead.
         */
        CoseEncrypt0 = 7,
        Invalid = -1,
    };

    EncString() = default;

    static EncString parse(const QString &encoded);
    static bool isRsaType(Type type);
    /// False for formats we can recognise but not decrypt, such as CoseEncrypt0.
    static bool isSupportedType(Type type);

    bool isNull() const { return m_type == Invalid; }
    /**
     * True when this is a well-formed value in a format KVault cannot read, as
     * opposed to corrupt data. Lets the UI say something more useful than
     * "could not be decrypted".
     */
    bool isUnsupportedFormat() const { return m_type != Invalid && !isSupportedType(m_type); }
    Type type() const { return m_type; }
    QString toString() const;

    /// Decrypt with a symmetric key; verifies the MAC when the type has one.
    std::optional<SecureBytes> decrypt(const SymmetricKey &key) const;
    /// Convenience wrapper returning UTF-8 text.
    std::optional<QString> decryptString(const SymmetricKey &key) const;

    /// Decrypt an RSA-wrapped value (used for organisation keys).
    std::optional<SecureBytes> decryptRsa(const SecureBytes &pkcs8PrivateKey) const;

    /// Encrypt as type 2 (or type 0 when the key has no MAC key).
    static EncString encrypt(const SecureBytes &plaintext, const SymmetricKey &key);
    static EncString encryptString(const QString &plaintext, const SymmetricKey &key);

    /**
     * Attachments and other binary blobs use a packed representation rather
     * than the base64 string form:
     *
     *     [1 byte type][16 byte iv][32 byte mac, if the type has one][ciphertext]
     */
    static EncString fromPackedBuffer(const QByteArray &buffer);
    QByteArray toPackedBuffer() const;

private:
    Type m_type = Invalid;
    QByteArray m_iv;
    QByteArray m_ciphertext;
    QByteArray m_mac;
};

} // namespace kvault
