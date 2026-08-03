#include "vaultkeys.h"

#include "crypto/encstring.h"

#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(KVAULT_VAULT, "kvault.vault")

namespace kvault {

VaultKeys::~VaultKeys()
{
    lock();
}

bool VaultKeys::unlock(const QString &email, const SecureBytes &password, const KdfConfig &kdf, const QString &wrappedUserKey)
{
    const auto masterKey = AccountCrypto::deriveMasterKey(email, password, kdf);
    if (!masterKey) {
        qCWarning(KVAULT_VAULT) << "Could not derive master key";
        return false;
    }
    return unlockWithMasterKey(*masterKey, wrappedUserKey);
}

bool VaultKeys::unlockWithMasterKey(const SecureBytes &masterKey, const QString &wrappedUserKey)
{
    lock();

    const SymmetricKey stretched = SymmetricKey::stretch(masterKey);
    if (!stretched.isValid()) {
        return false;
    }

    const EncString wrapped = EncString::parse(wrappedUserKey);
    if (wrapped.isNull()) {
        qCWarning(KVAULT_VAULT) << "Stored account key is malformed";
        return false;
    }

    // A wrong password fails the MAC check rather than yielding a bad key.
    auto material = wrapped.decrypt(stretched);
    if (!material) {
        return false;
    }

    m_userKey = SymmetricKey(std::move(*material));
    return m_userKey.isValid();
}

bool VaultKeys::loadPrivateKey(const QString &wrappedPrivateKey)
{
    if (wrappedPrivateKey.isEmpty() || !isUnlocked()) {
        return false;
    }
    const EncString wrapped = EncString::parse(wrappedPrivateKey);
    if (wrapped.isNull()) {
        return false;
    }
    auto key = wrapped.decrypt(m_userKey);
    if (!key) {
        qCWarning(KVAULT_VAULT) << "Could not decrypt account private key";
        return false;
    }
    m_privateKey = std::move(*key);
    return true;
}

void VaultKeys::loadOrganizationKeys(const QJsonArray &organizations)
{
    m_organizationKeys.clear();
    if (m_privateKey.isEmpty()) {
        if (!organizations.isEmpty()) {
            qCWarning(KVAULT_VAULT) << "No private key available; organization items will not decrypt";
        }
        return;
    }

    for (const QJsonValue &value : organizations) {
        const QJsonObject organization = value.toObject();
        const QString id = organization.value(QStringLiteral("id")).toString();
        const QString wrappedKey = organization.value(QStringLiteral("key")).toString();
        if (id.isEmpty() || wrappedKey.isEmpty()) {
            continue;
        }

        const EncString wrapped = EncString::parse(wrappedKey);
        auto material = wrapped.decryptRsa(m_privateKey);
        if (!material) {
            qCWarning(KVAULT_VAULT) << "Could not decrypt key for organization" << id;
            continue;
        }
        SymmetricKey key(std::move(*material));
        if (key.isValid()) {
            m_organizationKeys.insert(id, std::move(key));
        }
    }
}

bool VaultKeys::hasKeyForOrganization(const QString &organizationId) const
{
    return organizationId.isEmpty() ? isUnlocked() : m_organizationKeys.contains(organizationId);
}

const SymmetricKey &VaultKeys::keyForOrganization(const QString &organizationId) const
{
    if (organizationId.isEmpty()) {
        return m_userKey;
    }
    const auto it = m_organizationKeys.constFind(organizationId);
    return it == m_organizationKeys.constEnd() ? m_invalidKey : it.value();
}

void VaultKeys::lock()
{
    m_userKey.clear();
    m_privateKey.clear();
    for (auto &key : m_organizationKeys) {
        key.clear();
    }
    m_organizationKeys.clear();
}

} // namespace kvault
