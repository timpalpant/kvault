#pragma once

#include "crypto/kdf.h"
#include "crypto/symmetrickey.h"

#include <QHash>
#include <QJsonArray>
#include <QString>

namespace kvault {

/**
 * Holds the decrypted key material for an unlocked vault.
 *
 * Everything here is wiped by lock(), which is what makes locking meaningful:
 * the on-disk cache stays, but nothing can read it any more.
 */
class VaultKeys
{
public:
    VaultKeys() = default;
    ~VaultKeys();

    VaultKeys(const VaultKeys &) = delete;
    VaultKeys &operator=(const VaultKeys &) = delete;

    /**
     * Unwrap the user key using the master password.
     *
     * @param wrappedUserKey the account's "Key" field.
     * @return false if the password is wrong, which shows up as a MAC failure.
     */
    bool unlock(const QString &email, const SecureBytes &password, const KdfConfig &kdf, const QString &wrappedUserKey);

    /**
     * Same, but for when the master key has already been derived, as during
     * login where it was needed to compute the password hash.
     */
    bool unlockWithMasterKey(const SecureBytes &masterKey, const QString &wrappedUserKey);

    /// Decrypt the account's RSA private key, needed for organisation keys.
    bool loadPrivateKey(const QString &wrappedPrivateKey);

    /// Decrypt the per-organisation keys listed in the sync profile.
    void loadOrganizationKeys(const QJsonArray &organizations);

    /**
     * The key that protects a given item.
     * Personal items use the user key; shared items use their organisation key.
     */
    const SymmetricKey &keyForOrganization(const QString &organizationId) const;
    bool hasKeyForOrganization(const QString &organizationId) const;

    const SymmetricKey &userKey() const { return m_userKey; }
    bool isUnlocked() const { return m_userKey.isValid(); }

    void lock();

private:
    SymmetricKey m_userKey;
    SecureBytes m_privateKey; ///< PKCS#8 DER
    QHash<QString, SymmetricKey> m_organizationKeys;
    SymmetricKey m_invalidKey;
};

} // namespace kvault
