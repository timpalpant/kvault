#pragma once

#include "crypto/kdf.h"

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>
#include <optional>

namespace kvault {

/**
 * The account details needed to unlock offline.
 *
 * None of this is secret on its own: the wrapped keys are useless without the
 * master password, which is never written anywhere.
 */
struct StoredAccount {
    QString email;
    QString serverUrl;
    QString deviceIdentifier;
    QString wrappedUserKey;    ///< the account "Key", protected by the stretched master key
    QString wrappedPrivateKey; ///< RSA key, protected by the user key
    KdfConfig kdf;
    QDateTime lastSync;

    bool isValid() const;
};

/**
 * On-disk cache.
 *
 * The sync payload is stored exactly as the server sent it, i.e. still
 * encrypted. Nothing readable is written to disk.
 */
class LocalStore
{
public:
    static QString dataDirectory();

    bool saveAccount(const StoredAccount &account) const;
    std::optional<StoredAccount> loadAccount() const;

    bool saveSyncPayload(const QJsonObject &payload) const;
    std::optional<QJsonObject> loadSyncPayload() const;

    /// Forget everything, including the cached vault.
    void clear() const;
    bool hasCachedVault() const;

private:
    QString accountPath() const;
    QString vaultPath() const;
    static bool writeJson(const QString &path, const QJsonObject &json);
    static std::optional<QJsonObject> readJson(const QString &path);
};

/// The session as read back from the wallet.
struct StoredTokens {
    bool found = false;
    QString email;
    QString accessToken;
    QString refreshToken;
    QDateTime expiry;
};

/**
 * Session tokens, kept in the platform secret store (KWallet or the
 * Secret Service) rather than on disk.
 */
class TokenStore : public QObject
{
    Q_OBJECT

public:
    explicit TokenStore(QObject *parent = nullptr);

    /// @p email binds the token to an account so it cannot be reused for another.
    void save(const QString &email, const QString &accessToken, const QString &refreshToken, const QDateTime &expiry);

    /**
     * Read the stored session.
     *
     * Opening the wallet is a D-Bus round trip that can stall for its full
     * 25 second timeout when no secret service is answering, so this is
     * deliberately callback-driven and called only when a session is needed -
     * never on the startup path.
     */
    void load(std::function<void(const StoredTokens &)> handler);
    void clear();
};

} // namespace kvault
