#pragma once

#include "crypto/encstring.h"
#include "crypto/symmetrickey.h"

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

namespace kvault {

enum class CipherType {
    Login = 1,
    SecureNote = 2,
    Card = 3,
    Identity = 4,
    SshKey = 5,
};

enum class FieldType {
    Text = 0,
    Hidden = 1,
    Boolean = 2,
    Linked = 3,
};

enum class UriMatchType {
    Default = -1,
    Domain = 0,
    Host = 1,
    StartsWith = 2,
    Exact = 3,
    RegularExpression = 4,
    Never = 5,
};

enum class RepromptType {
    None = 0,
    Password = 1,
};

struct LoginUri {
    QString uri;
    int match = int(UriMatchType::Default);
};

struct CustomField {
    QString name;
    QString value;
    int type = int(FieldType::Text);
    int linkedId = -1;
};

struct PasswordHistoryEntry {
    QString password;
    QDateTime lastUsedDate;
};

struct AttachmentInfo {
    QString id;
    QString fileName;
    QString url;
    QString key; ///< EncString wrapping the per-attachment key
    qint64 size = 0;
    QString sizeName;
};

/**
 * A single vault item, holding decrypted values.
 *
 * Instances only exist while the vault is unlocked; VaultManager drops them all
 * on lock.
 */
class Cipher
{
public:
    QString id;
    QString organizationId;
    QString folderId;
    CipherType type = CipherType::Login;

    QString name;
    QString notes;
    bool favorite = false;
    int reprompt = int(RepromptType::None);
    bool edit = true;
    bool viewPassword = true;

    QDateTime creationDate;
    QDateTime revisionDate;
    QDateTime deletedDate;

    /// The item's own key, still wrapped. Preserved so edits round-trip.
    QString wrappedItemKey;

    // --- Login ---
    QString username;
    QString password;
    QString totp;
    QDateTime passwordRevisionDate;
    QList<LoginUri> uris;

    // --- Card ---
    QString cardholderName;
    QString cardBrand;
    QString cardNumber;
    QString cardExpMonth;
    QString cardExpYear;
    QString cardCode;

    // --- Identity ---
    QString identityTitle;
    QString firstName;
    QString middleName;
    QString lastName;
    QString address1;
    QString address2;
    QString address3;
    QString city;
    QString state;
    QString postalCode;
    QString country;
    QString company;
    QString email;
    QString phone;
    QString ssn;
    QString identityUsername;
    QString passportNumber;
    QString licenseNumber;

    // --- SSH key ---
    QString sshPrivateKey;
    QString sshPublicKey;
    QString sshFingerprint;

    // --- Secure note ---
    int secureNoteType = 0;

    QList<CustomField> fields;
    QList<AttachmentInfo> attachments;
    QList<PasswordHistoryEntry> passwordHistory;

    /// Set when at least one field could not be decrypted.
    bool decryptionFailed = false;
    /// Set when a field uses an encryption format KVault does not implement.
    bool usesUnsupportedEncryption = false;

    bool isInTrash() const { return deletedDate.isValid(); }
    bool hasTotp() const { return !totp.isEmpty(); }

    /// A one-line summary for list views: username, card number, or full name.
    QString subtitle() const;

    /**
     * Decrypt a cipher from the sync payload.
     * @param containerKey the user key, or the organization key for shared items.
     */
    static std::optional<Cipher> fromEncryptedJson(const QJsonObject &json, const SymmetricKey &containerKey);

    /**
     * Build the request body for POST /ciphers or PUT /ciphers/{id}.
     * Returns an empty object if encryption fails.
     */
    QJsonObject toEncryptedJson(const SymmetricKey &containerKey) const;

    /// Resolve the key that protects this item's fields.
    static std::optional<SymmetricKey> resolveItemKey(const QString &wrappedKey, const SymmetricKey &containerKey);
};

/// Parse Bitwarden's ISO-8601 timestamps, which carry more sub-second digits than Qt accepts.
QDateTime parseBitwardenDate(const QString &text);
QString formatBitwardenDate(const QDateTime &dateTime);

} // namespace kvault
