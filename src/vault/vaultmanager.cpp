#include "vaultmanager.h"

#include "app/appsettings.h"
#include "attachmentmanager.h"
#include "ciphereditor.h"
#include "cipherfilterproxymodel.h"
#include "cipherlistmodel.h"
#include "crypto/totp.h"
#include "foldermodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QtConcurrent>

#include <KLocalizedString>

Q_DECLARE_LOGGING_CATEGORY(KVAULT_VAULT)

namespace kvault {

VaultManager::VaultManager(QObject *parent)
    : QObject(parent)
    , m_api(new ApiClient(this))
    , m_settings(new AppSettings(this))
    , m_tokens(new TokenStore(this))
    , m_ciphers(new CipherListModel(this))
    , m_filtered(new CipherFilterProxyModel(this))
    , m_folders(new FolderModel(this))
    , m_editor(new CipherEditor(this))
{
    m_filtered->setSourceModel(m_ciphers);
    m_attachments = new AttachmentManager(m_api, &m_keys, m_ciphers, this);

    m_api->setDeviceIdentifier(m_settings->deviceIdentifier());

    connect(m_ciphers, &CipherListModel::countsChanged, this, &VaultManager::refreshFolderCounts);

    connect(m_api, &ApiClient::sessionExpired, this, [this]() {
        // The refresh token is gone, so the cached vault can no longer be
        // refreshed; keep it readable but force a new login for anything else.
        m_tokens->clear();
        Q_EMIT errorOccurred(i18n("Your session expired. Please log in again."));
        logout();
    });
    connect(m_api, &ApiClient::tokensRefreshed, this, [this]() {
        m_tokens->save(m_account.email, m_api->accessToken(), m_api->refreshToken(), m_api->tokenExpiry());
    });

    m_inactivityTimer.setSingleShot(true);
    connect(&m_inactivityTimer, &QTimer::timeout, this, [this]() {
        if (m_state == Unlocked) {
            qCInfo(KVAULT_VAULT) << "Locking after inactivity";
            lock();
        }
    });
    connect(m_settings, &AppSettings::lockTimeoutMinutesChanged, this, &VaultManager::resetInactivityTimer);

    restoreSession();
}

VaultManager::~VaultManager()
{
    clearSensitiveState();
}

void VaultManager::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    resetInactivityTimer();
    Q_EMIT stateChanged();
}

void VaultManager::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

void VaultManager::setSyncing(bool syncing)
{
    if (m_syncing == syncing) {
        return;
    }
    m_syncing = syncing;
    Q_EMIT syncingChanged();
}

void VaultManager::setOffline(bool offline)
{
    if (m_offline == offline) {
        return;
    }
    m_offline = offline;
    Q_EMIT offlineChanged();
}

void VaultManager::restoreSession()
{
    const auto stored = m_store.loadAccount();
    if (!stored) {
        setState(LoggedOut);
        return;
    }

    m_account = *stored;
    m_api->setServerUrl(m_account.serverUrl);
    if (!m_account.deviceIdentifier.isEmpty()) {
        m_api->setDeviceIdentifier(m_account.deviceIdentifier);
    }
    Q_EMIT accountChanged();

    setState(Locked);
}

// ---------------------------------------------------------------------------
// Login
// ---------------------------------------------------------------------------

void VaultManager::login(const QString &email, const QString &password, const QString &serverUrl)
{
    if (email.isEmpty() || password.isEmpty()) {
        Q_EMIT loginFailed(i18n("Enter your email address and master password."));
        return;
    }

    m_api->setServerUrl(serverUrl);
    m_settings->setServerUrl(m_api->serverUrl());
    m_settings->setLastEmail(email);

    m_pendingToken = TokenRequest{};
    m_pendingToken.email = email;
    m_pendingPassword = SecureBytes::fromString(password);
    setBusy(true);

    m_api->prelogin(email, [this](std::optional<KdfConfig> kdf, const ApiResponse &response) {
        if (!kdf) {
            setBusy(false);
            m_pendingPassword.clear();
            Q_EMIT loginFailed(response.networkError ? i18n("Could not reach the server: %1", response.errorMessage) : response.errorMessage);
            return;
        }
        m_pendingKdf = *kdf;
        continueLogin();
    });
}

void VaultManager::deriveMasterKeyAsync(const QString &email,
                                        const SecureBytes &password,
                                        const KdfConfig &kdf,
                                        std::function<void(std::optional<SecureBytes>)> then)
{
    auto *watcher = new QFutureWatcher<std::optional<SecureBytes>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [watcher, then = std::move(then)]() {
        then(watcher->result());
        watcher->deleteLater();
    });
    // The copy of the password captured here is wiped when the task ends.
    watcher->setFuture(QtConcurrent::run([email, password, kdf]() { return AccountCrypto::deriveMasterKey(email, password, kdf); }));
}

void VaultManager::withStoredSession(std::function<void()> then)
{
    if (m_sessionRead) {
        then();
        return;
    }

    m_tokens->load([this, then = std::move(then)](const StoredTokens &tokens) {
        m_sessionRead = true;

        // A token from a different account must never be used against this
        // vault: it would sync someone else's data over the stored one. An
        // entry carrying no email cannot be attributed to an account at all, so
        // it is discarded on the same grounds; unlock() re-authenticates with
        // the master password rather than making the user log in again.
        if (tokens.found) {
            if (!tokens.email.isEmpty() && AccountCrypto::normaliseEmail(tokens.email) == AccountCrypto::normaliseEmail(m_account.email)) {
                m_api->setTokens(tokens.accessToken, tokens.refreshToken, tokens.expiry);
            } else {
                qCWarning(KVAULT_VAULT) << "Stored session does not belong to this account, discarding it";
                m_tokens->clear();
            }
        }

        then();
    });
}

void VaultManager::continueLogin()
{
    deriveMasterKeyAsync(m_pendingToken.email, m_pendingPassword, m_pendingKdf, [this](std::optional<SecureBytes> masterKey) {
        if (!masterKey) {
            m_pendingPassword.clear();
            setBusy(false);
            Q_EMIT loginFailed(i18n("Could not derive the account key."));
            return;
        }

        m_pendingMasterKey = *masterKey;
        m_pendingToken.masterPasswordHash
            = AccountCrypto::hashMasterPassword(m_pendingMasterKey, m_pendingPassword, AccountCrypto::HashPurpose::ServerAuthorization);
        // The password itself is no longer needed; only the derived values are.
        m_pendingPassword.clear();

        submitPendingToken();
    });
}

void VaultManager::submitPendingToken()
{
    setBusy(true);

    m_api->requestToken(m_pendingToken, [this](const ApiResponse &response, const LoginChallenge &challenge) {
        if (response.ok) {
            completeLogin(response);
            return;
        }
        setBusy(false);

        switch (challenge.kind) {
        case LoginChallenge::TwoFactorRequired:
            if (m_pendingToken.twoFactorCode.isEmpty()) {
                Q_EMIT twoFactorRequired(challenge.twoFactorProviders);
            } else {
                // Asked again after we supplied one: the code was wrong.
                m_pendingToken.twoFactorCode.clear();
                Q_EMIT loginFailed(i18n("That code was not accepted. Try again."));
            }
            return;

        case LoginChallenge::NewDeviceVerification:
            if (m_pendingToken.newDeviceOtp.isEmpty()) {
                Q_EMIT newDeviceVerificationRequired(challenge.message);
            } else {
                m_pendingToken.newDeviceOtp.clear();
                Q_EMIT loginFailed(i18n("That verification code was not accepted. Try again."));
            }
            return;

        case LoginChallenge::CaptchaRequired:
        case LoginChallenge::ClientTooOld:
            clearSensitiveState();
            Q_EMIT loginFailed(challenge.message);
            return;

        case LoginChallenge::None:
            break;
        }

        clearSensitiveState();

        if (m_state == Unlocked) {
            // This was a background re-authentication, not a login attempt. The
            // vault is readable from cache; just mark the session unusable.
            setOffline(true);
            Q_EMIT syncFinished(false,
                                response.networkError ? i18n("Working offline: %1", response.errorMessage)
                                                      : i18n("Could not refresh your session: %1", response.errorMessage));
            return;
        }

        Q_EMIT loginFailed(response.networkError ? i18n("Could not reach the server: %1", response.errorMessage) : response.errorMessage);
    });
}

void VaultManager::submitTwoFactor(const QString &code, int provider, bool rememberDevice)
{
    if (m_pendingToken.masterPasswordHash.isEmpty()) {
        Q_EMIT loginFailed(i18n("The login attempt expired. Please start again."));
        return;
    }
    m_pendingToken.twoFactorCode = code.trimmed();
    m_pendingToken.twoFactorProvider = TwoFactorProvider(provider);
    m_pendingToken.rememberDevice = rememberDevice;
    submitPendingToken();
}

void VaultManager::submitNewDeviceCode(const QString &code)
{
    if (m_pendingToken.masterPasswordHash.isEmpty()) {
        Q_EMIT loginFailed(i18n("The login attempt expired. Please start again."));
        return;
    }
    if (code.trimmed().isEmpty()) {
        Q_EMIT loginFailed(i18n("Enter the verification code from your email."));
        return;
    }
    m_pendingToken.newDeviceOtp = code.trimmed();
    submitPendingToken();
}

void VaultManager::resendNewDeviceCode()
{
    if (m_pendingToken.masterPasswordHash.isEmpty()) {
        return;
    }
    // Retrying without a code makes the server send a new one.
    m_pendingToken.newDeviceOtp.clear();
    submitPendingToken();
    Q_EMIT operationSucceeded(i18n("A new verification code has been emailed to you."));
}

void VaultManager::requestEmailCode()
{
    if (m_pendingToken.masterPasswordHash.isEmpty()) {
        return;
    }
    m_api->sendEmailLoginCode(m_pendingToken.email, m_pendingToken.masterPasswordHash, [this](const ApiResponse &response) {
        if (response.ok) {
            Q_EMIT operationSucceeded(i18n("A login code has been emailed to you."));
        } else {
            Q_EMIT errorOccurred(response.errorMessage);
        }
    });
}

void VaultManager::cancelLogin()
{
    clearSensitiveState();
    setBusy(false);
}

void VaultManager::completeLogin(const ApiResponse &response)
{
    const QString wrappedUserKey = response.json.value(QStringLiteral("Key")).toString();
    const QString wrappedPrivateKey = response.json.value(QStringLiteral("PrivateKey")).toString();

    if (wrappedUserKey.isEmpty()) {
        setBusy(false);
        clearSensitiveState();
        Q_EMIT loginFailed(i18n("The server did not return an account key. "
                                "Accounts without a master password are not supported."));
        return;
    }

    // The token response repeats the KDF settings; trust those over prelogin.
    const KdfConfig kdf = KdfConfig::fromJson(response.json);
    m_pendingKdf = kdf.isValid() ? kdf : m_pendingKdf;

    if (!m_keys.unlockWithMasterKey(m_pendingMasterKey, wrappedUserKey)) {
        setBusy(false);
        clearSensitiveState();
        Q_EMIT loginFailed(i18n("Could not unlock the account key with this password."));
        return;
    }
    m_keys.loadPrivateKey(wrappedPrivateKey);

    m_account.email = m_pendingToken.email;
    m_account.serverUrl = m_api->serverUrl();
    m_account.deviceIdentifier = m_settings->deviceIdentifier();
    m_account.wrappedUserKey = wrappedUserKey;
    m_account.wrappedPrivateKey = wrappedPrivateKey;
    m_account.kdf = m_pendingKdf;

    m_tokens->save(m_account.email, m_api->accessToken(), m_api->refreshToken(), m_api->tokenExpiry());
    persistAccount();

    clearSensitiveState();
    setBusy(false);
    setState(Unlocked);
    Q_EMIT accountChanged();

    sync();
}

// ---------------------------------------------------------------------------
// Lock and unlock
// ---------------------------------------------------------------------------

void VaultManager::unlock(const QString &password)
{
    if (m_state == Unlocked) {
        return;
    }
    if (!m_account.isValid()) {
        Q_EMIT unlockFailed(i18n("No account is stored on this device."));
        return;
    }
    if (password.isEmpty()) {
        Q_EMIT unlockFailed(i18n("Enter your master password."));
        return;
    }

    setBusy(true);
    // Kept alive for the callback: if the stored session has gone, the password
    // is what lets us get a new one without discarding the offline vault.
    const SecureBytes secret = SecureBytes::fromString(password);

    deriveMasterKeyAsync(m_account.email, secret, m_account.kdf, [this, secret](std::optional<SecureBytes> masterKey) {
        setBusy(false);

        // A wrong password shows up as a MAC failure when unwrapping the key,
        // not as a derivation failure.
        if (!masterKey || !m_keys.unlockWithMasterKey(*masterKey, m_account.wrappedUserKey)) {
            Q_EMIT unlockFailed(i18n("That master password is not correct."));
            return;
        }

        m_keys.loadPrivateKey(m_account.wrappedPrivateKey);

        const auto cached = m_store.loadSyncPayload();
        if (cached) {
            applySyncPayload(*cached);
        }

        setState(Unlocked);

        if (!m_settings->syncOnUnlock()) {
            // The user asked not to contact the server when unlocking, which
            // covers re-authentication too.
            return;
        }

        withStoredSession([this, masterKey, secret]() {
            if (!m_api->hasTokens()) {
                // The session expired or was never stored. Re-authenticate with the
                // password just entered rather than making the user log out, which
                // would throw away the offline copy of the vault.
                qCInfo(KVAULT_VAULT) << "No usable session, re-authenticating with the master password";
                m_pendingToken = TokenRequest{};
                m_pendingToken.email = m_account.email;
                m_pendingMasterKey = *masterKey;
                m_pendingToken.masterPasswordHash
                    = AccountCrypto::hashMasterPassword(m_pendingMasterKey, secret, AccountCrypto::HashPurpose::ServerAuthorization);
                submitPendingToken();
                return;
            }

            sync();
        });
    });
}

void VaultManager::lock()
{
    m_keys.lock();
    m_ciphers->clear();
    m_folders->clear();
    m_editor->reset(CipherType::Login);
    m_inactivityTimer.stop();

    if (m_account.isValid()) {
        setState(Locked);
    } else {
        setState(LoggedOut);
    }
}

void VaultManager::logout()
{
    clearSensitiveState();
    m_keys.lock();
    m_ciphers->clear();
    m_folders->clear();

    m_api->clearTokens();
    m_tokens->clear();
    m_store.clear();
    m_account = StoredAccount();

    setState(LoggedOut);
    Q_EMIT accountChanged();
}

void VaultManager::clearSensitiveState()
{
    m_pendingPassword.clear();
    m_pendingMasterKey.clear();
    m_pendingToken = TokenRequest{};
}

void VaultManager::noteActivity()
{
    if (m_state == Unlocked && m_inactivityTimer.isActive()) {
        m_inactivityTimer.start();
    }
}

void VaultManager::resetInactivityTimer()
{
    const int minutes = m_settings->lockTimeoutMinutes();
    if (m_state != Unlocked || minutes <= 0) {
        m_inactivityTimer.stop();
        return;
    }
    m_inactivityTimer.start(minutes * 60 * 1000);
}

// ---------------------------------------------------------------------------
// Sync
// ---------------------------------------------------------------------------

void VaultManager::sync()
{
    if (m_state != Unlocked) {
        return;
    }

    // Syncing may be the first thing that needs a session, for instance when
    // unlocking was configured not to contact the server.
    withStoredSession([this]() {
        if (!m_api->hasTokens()) {
            setOffline(true);
            Q_EMIT syncFinished(false, i18n("Not signed in to the server."));
            return;
        }
        performSync();
    });
}

void VaultManager::performSync()
{
    setSyncing(true);
    m_api->sync([this](const ApiResponse &response) {
        setSyncing(false);

        if (!response.ok) {
            setOffline(true);
            const QString message = response.networkError ? i18n("Working offline: %1", response.errorMessage) : response.errorMessage;
            Q_EMIT syncFinished(false, message);
            return;
        }

        setOffline(false);
        m_store.saveSyncPayload(response.json);
        applySyncPayload(response.json);

        m_account.lastSync = QDateTime::currentDateTimeUtc();
        persistAccount();

        Q_EMIT syncFinished(true, i18n("Vault updated."));
    });
}

void VaultManager::applySyncPayload(const QJsonObject &payload)
{
    if (!m_keys.isUnlocked()) {
        return;
    }

    const QJsonObject profile = payload.value(QStringLiteral("profile")).toObject();

    // A rotated key or a first sync after login may bring a private key we do
    // not have yet.
    const QString privateKey = profile.value(QStringLiteral("privateKey")).toString();
    if (!privateKey.isEmpty() && privateKey != m_account.wrappedPrivateKey) {
        m_account.wrappedPrivateKey = privateKey;
        m_keys.loadPrivateKey(privateKey);
        persistAccount();
    }
    m_keys.loadOrganizationKeys(profile.value(QStringLiteral("organizations")).toArray());

    QList<Folder> folders;
    for (const QJsonValue &value : payload.value(QStringLiteral("folders")).toArray()) {
        const auto folder = Folder::fromEncryptedJson(value.toObject(), m_keys.userKey());
        if (folder) {
            folders.append(*folder);
        }
    }
    m_folders->setFolders(std::move(folders));

    QList<Cipher> ciphers;
    int failed = 0;
    int unsupportedFormat = 0;
    for (const QJsonValue &value : payload.value(QStringLiteral("ciphers")).toArray()) {
        const QJsonObject object = value.toObject();
        const QString organizationId = object.value(QStringLiteral("organizationId")).toString();

        if (!m_keys.hasKeyForOrganization(organizationId)) {
            // Shared item whose organisation key we could not unwrap.
            ++failed;
            continue;
        }
        auto cipher = Cipher::fromEncryptedJson(object, m_keys.keyForOrganization(organizationId));
        if (!cipher) {
            ++failed;
            continue;
        }
        if (cipher->usesUnsupportedEncryption) {
            ++unsupportedFormat;
        }
        ciphers.append(*cipher);
    }
    m_ciphers->setCiphers(std::move(ciphers));

    // Worth separating: a newer ciphertext format is a known gap in this client,
    // not corruption, and the fix is different.
    if (unsupportedFormat > 0) {
        qCWarning(KVAULT_VAULT) << unsupportedFormat << "items use the newer COSE encryption format";
        Q_EMIT errorOccurred(i18np("%1 item uses a newer encryption format that KVault cannot read yet.",
                                   "%1 items use a newer encryption format that KVault cannot read yet.",
                                   unsupportedFormat));
    }
    if (failed > unsupportedFormat) {
        const int corrupt = failed;
        qCWarning(KVAULT_VAULT) << corrupt << "items could not be decrypted";
        Q_EMIT errorOccurred(
            i18np("%1 item could not be decrypted and is not shown.", "%1 items could not be decrypted and are not shown.", corrupt));
    }
}

void VaultManager::refreshFolderCounts()
{
    QHash<QString, int> counts;
    for (const Folder &folder : m_folders->folders()) {
        counts.insert(folder.id, m_ciphers->countInFolder(folder.id));
    }
    m_folders->setItemCounts(counts);
}

void VaultManager::persistAccount()
{
    if (m_account.isValid()) {
        m_store.saveAccount(m_account);
    }
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------

const Cipher *VaultManager::requireCipher(const QString &cipherId) const
{
    const Cipher *cipher = m_ciphers->cipherById(cipherId);
    if (!cipher) {
        qCWarning(KVAULT_VAULT) << "No such item:" << cipherId;
    }
    return cipher;
}

QVariantMap VaultManager::cipherDetails(const QString &cipherId) const
{
    const Cipher *cipher = m_ciphers->cipherById(cipherId);
    if (!cipher) {
        return {};
    }

    QVariantList uris;
    for (const LoginUri &uri : cipher->uris) {
        uris.append(QVariantMap{{QStringLiteral("uri"), uri.uri}, {QStringLiteral("match"), uri.match}});
    }

    QVariantList fields;
    for (const CustomField &field : cipher->fields) {
        fields.append(QVariantMap{
            {QStringLiteral("name"), field.name},
            {QStringLiteral("value"), field.value},
            {QStringLiteral("type"), field.type},
        });
    }

    QVariantList attachments;
    for (const AttachmentInfo &attachment : cipher->attachments) {
        attachments.append(QVariantMap{
            {QStringLiteral("id"), attachment.id},
            {QStringLiteral("fileName"), attachment.fileName},
            {QStringLiteral("size"), attachment.size},
            {QStringLiteral("sizeName"), attachment.sizeName},
        });
    }

    QVariantList history;
    for (const PasswordHistoryEntry &entry : cipher->passwordHistory) {
        history.append(QVariantMap{
            {QStringLiteral("password"), entry.password},
            {QStringLiteral("lastUsedDate"), entry.lastUsedDate},
        });
    }

    return {
        {QStringLiteral("id"), cipher->id},
        {QStringLiteral("type"), int(cipher->type)},
        {QStringLiteral("name"), cipher->name},
        {QStringLiteral("notes"), cipher->notes},
        {QStringLiteral("favorite"), cipher->favorite},
        {QStringLiteral("reprompt"), cipher->reprompt},
        {QStringLiteral("folderId"), cipher->folderId},
        {QStringLiteral("organizationId"), cipher->organizationId},
        {QStringLiteral("editable"), cipher->edit},
        {QStringLiteral("inTrash"), cipher->isInTrash()},
        {QStringLiteral("decryptionFailed"), cipher->decryptionFailed},
        {QStringLiteral("creationDate"), cipher->creationDate},
        {QStringLiteral("revisionDate"), cipher->revisionDate},

        {QStringLiteral("username"), cipher->username},
        {QStringLiteral("password"), cipher->password},
        {QStringLiteral("totp"), cipher->totp},
        {QStringLiteral("passwordRevisionDate"), cipher->passwordRevisionDate},
        {QStringLiteral("uris"), uris},

        {QStringLiteral("cardholderName"), cipher->cardholderName},
        {QStringLiteral("cardBrand"), cipher->cardBrand},
        {QStringLiteral("cardNumber"), cipher->cardNumber},
        {QStringLiteral("cardExpMonth"), cipher->cardExpMonth},
        {QStringLiteral("cardExpYear"), cipher->cardExpYear},
        {QStringLiteral("cardCode"), cipher->cardCode},

        {QStringLiteral("identityTitle"), cipher->identityTitle},
        {QStringLiteral("firstName"), cipher->firstName},
        {QStringLiteral("middleName"), cipher->middleName},
        {QStringLiteral("lastName"), cipher->lastName},
        {QStringLiteral("address1"), cipher->address1},
        {QStringLiteral("address2"), cipher->address2},
        {QStringLiteral("address3"), cipher->address3},
        {QStringLiteral("city"), cipher->city},
        {QStringLiteral("state"), cipher->state},
        {QStringLiteral("postalCode"), cipher->postalCode},
        {QStringLiteral("country"), cipher->country},
        {QStringLiteral("company"), cipher->company},
        {QStringLiteral("email"), cipher->email},
        {QStringLiteral("phone"), cipher->phone},
        {QStringLiteral("ssn"), cipher->ssn},
        {QStringLiteral("identityUsername"), cipher->identityUsername},
        {QStringLiteral("passportNumber"), cipher->passportNumber},
        {QStringLiteral("licenseNumber"), cipher->licenseNumber},

        {QStringLiteral("sshPrivateKey"), cipher->sshPrivateKey},
        {QStringLiteral("sshPublicKey"), cipher->sshPublicKey},
        {QStringLiteral("sshFingerprint"), cipher->sshFingerprint},

        {QStringLiteral("fields"), fields},
        {QStringLiteral("attachments"), attachments},
        {QStringLiteral("passwordHistory"), history},
    };
}

QString VaultManager::currentTotpCode(const QString &cipherId) const
{
    const Cipher *cipher = m_ciphers->cipherById(cipherId);
    if (!cipher || cipher->totp.isEmpty()) {
        return {};
    }
    const Totp totp = Totp::parse(cipher->totp);
    return totp.isValid() ? totp.code(QDateTime::currentSecsSinceEpoch()) : QString();
}

void VaultManager::beginCreate(int cipherType, const QString &folderId)
{
    m_editor->reset(CipherType(cipherType), folderId);
}

bool VaultManager::beginEdit(const QString &cipherId)
{
    const Cipher *cipher = requireCipher(cipherId);
    if (!cipher) {
        return false;
    }
    m_editor->loadFrom(*cipher);
    return true;
}

void VaultManager::saveEditor()
{
    const Cipher cipher = m_editor->toCipher();
    if (cipher.name.trimmed().isEmpty()) {
        Q_EMIT errorOccurred(i18n("Give the item a name before saving."));
        return;
    }
    submitCipher(cipher, cipher.id.isEmpty());
}

void VaultManager::submitCipher(const Cipher &cipher, bool isNew)
{
    if (!m_keys.hasKeyForOrganization(cipher.organizationId)) {
        Q_EMIT errorOccurred(i18n("The vault is locked."));
        return;
    }

    const QJsonObject body = cipher.toEncryptedJson(m_keys.keyForOrganization(cipher.organizationId));
    if (body.isEmpty()) {
        Q_EMIT errorOccurred(i18n("Could not encrypt the item."));
        return;
    }

    setBusy(true);
    const auto handler = [this, isNew](const ApiResponse &response) {
        setBusy(false);
        if (!response.ok) {
            Q_EMIT errorOccurred(response.errorMessage);
            return;
        }

        // The response is the saved item, so decrypt it back rather than
        // guessing what the server stored.
        const QString organizationId = response.json.value(QStringLiteral("organizationId")).toString();
        if (m_keys.hasKeyForOrganization(organizationId)) {
            const auto saved = Cipher::fromEncryptedJson(response.json, m_keys.keyForOrganization(organizationId));
            if (saved) {
                m_ciphers->upsert(*saved);
                Q_EMIT cipherSaved(saved->id);
            }
        }
        Q_EMIT operationSucceeded(isNew ? i18n("Item created.") : i18n("Item saved."));
    };

    if (isNew) {
        m_api->createCipher(body, handler);
    } else {
        m_api->updateCipher(cipher.id, body, handler);
    }
}

void VaultManager::setFavorite(const QString &cipherId, bool favorite)
{
    const Cipher *existing = requireCipher(cipherId);
    if (!existing || existing->favorite == favorite) {
        return;
    }
    Cipher updated = *existing;
    updated.favorite = favorite;
    submitCipher(updated, false);
}

void VaultManager::moveToTrash(const QString &cipherId)
{
    if (!requireCipher(cipherId)) {
        return;
    }
    setBusy(true);
    m_api->softDeleteCipher(cipherId, [this, cipherId](const ApiResponse &response) {
        setBusy(false);
        if (!response.ok) {
            Q_EMIT errorOccurred(response.errorMessage);
            return;
        }
        // The endpoint returns no body, so update the cached item ourselves.
        if (const Cipher *existing = m_ciphers->cipherById(cipherId)) {
            Cipher trashed = *existing;
            trashed.deletedDate = QDateTime::currentDateTimeUtc();
            m_ciphers->upsert(trashed);
        }
        Q_EMIT operationSucceeded(i18n("Moved to trash."));
    });
}

void VaultManager::restoreFromTrash(const QString &cipherId)
{
    if (!requireCipher(cipherId)) {
        return;
    }
    setBusy(true);
    m_api->restoreCipher(cipherId, [this, cipherId](const ApiResponse &response) {
        setBusy(false);
        if (!response.ok) {
            Q_EMIT errorOccurred(response.errorMessage);
            return;
        }
        if (const Cipher *existing = m_ciphers->cipherById(cipherId)) {
            Cipher restored = *existing;
            restored.deletedDate = QDateTime();
            m_ciphers->upsert(restored);
        }
        Q_EMIT operationSucceeded(i18n("Item restored."));
    });
}

void VaultManager::deleteForever(const QString &cipherId)
{
    if (!requireCipher(cipherId)) {
        return;
    }
    setBusy(true);
    m_api->deleteCipher(cipherId, [this, cipherId](const ApiResponse &response) {
        setBusy(false);
        if (!response.ok) {
            Q_EMIT errorOccurred(response.errorMessage);
            return;
        }
        m_ciphers->removeById(cipherId);
        Q_EMIT operationSucceeded(i18n("Item deleted permanently."));
    });
}

// ---------------------------------------------------------------------------
// Folders
// ---------------------------------------------------------------------------

void VaultManager::createFolder(const QString &name)
{
    if (name.trimmed().isEmpty() || !m_keys.isUnlocked()) {
        return;
    }
    Folder folder;
    folder.name = name.trimmed();

    setBusy(true);
    m_api->createFolder(folder.toEncryptedJson(m_keys.userKey()), [this](const ApiResponse &response) {
        setBusy(false);
        if (!response.ok) {
            Q_EMIT errorOccurred(response.errorMessage);
            return;
        }
        sync();
        Q_EMIT operationSucceeded(i18n("Folder created."));
    });
}

void VaultManager::renameFolder(const QString &folderId, const QString &name)
{
    if (folderId.isEmpty() || name.trimmed().isEmpty() || !m_keys.isUnlocked()) {
        return;
    }
    Folder folder;
    folder.id = folderId;
    folder.name = name.trimmed();

    setBusy(true);
    m_api->updateFolder(folderId, folder.toEncryptedJson(m_keys.userKey()), [this](const ApiResponse &response) {
        setBusy(false);
        if (!response.ok) {
            Q_EMIT errorOccurred(response.errorMessage);
            return;
        }
        sync();
        Q_EMIT operationSucceeded(i18n("Folder renamed."));
    });
}

void VaultManager::deleteFolder(const QString &folderId)
{
    if (folderId.isEmpty()) {
        return;
    }
    setBusy(true);
    m_api->deleteFolder(folderId, [this](const ApiResponse &response) {
        setBusy(false);
        if (!response.ok) {
            Q_EMIT errorOccurred(response.errorMessage);
            return;
        }
        // Items in the folder are not deleted, only unfiled, so re-sync.
        sync();
        Q_EMIT operationSucceeded(i18n("Folder deleted."));
    });
}

} // namespace kvault
