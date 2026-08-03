#pragma once

#include "api/apiclient.h"
#include "attachmentmanager.h"
#include "ciphereditor.h"
#include "cipherfilterproxymodel.h"
#include "cipherlistmodel.h"
#include "foldermodel.h"
#include "localstore.h"
#include "model/cipher.h"
#include "vaultkeys.h"

#include <QObject>
#include <QQmlEngine>
#include <QTimer>

namespace kvault {

// Included rather than forward-declared: qmltyperegistrar has to see the full
// types to expose them as property types to QML.
class AppSettings;

/**
 * The application's center of gravity.
 *
 * Owns the session, the key material and the decrypted vault, and is the only
 * thing QML talks to for anything that touches the account.
 */
class VaultManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString email READ email NOTIFY accountChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY accountChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(bool syncing READ isSyncing NOTIFY syncingChanged)
    Q_PROPERTY(QDateTime lastSync READ lastSync NOTIFY syncingChanged)
    Q_PROPERTY(bool offline READ isOffline NOTIFY offlineChanged)

    // Fully qualified: moc records the property type exactly as written, and
    // QML registers these under their namespaced names.
    Q_PROPERTY(kvault::CipherListModel *ciphers READ ciphers CONSTANT)
    Q_PROPERTY(kvault::CipherFilterProxyModel *filteredCiphers READ filteredCiphers CONSTANT)
    Q_PROPERTY(kvault::FolderModel *folders READ folders CONSTANT)
    Q_PROPERTY(kvault::CipherEditor *editor READ editor CONSTANT)
    Q_PROPERTY(kvault::AttachmentManager *attachments READ attachments CONSTANT)

public:
    enum State {
        LoggedOut, ///< no account stored
        Locked,    ///< account stored, keys not in memory
        Unlocked,  ///< vault readable
    };
    Q_ENUM(State)

    explicit VaultManager(QObject *parent = nullptr);
    ~VaultManager() override;

    State state() const { return m_state; }
    QString email() const { return m_account.email; }
    QString serverUrl() const { return m_account.serverUrl; }
    bool isBusy() const { return m_busy; }
    bool isSyncing() const { return m_syncing; }
    bool isOffline() const { return m_offline; }
    QDateTime lastSync() const { return m_account.lastSync; }

    CipherListModel *ciphers() const { return m_ciphers; }
    CipherFilterProxyModel *filteredCiphers() const { return m_filtered; }
    FolderModel *folders() const { return m_folders; }
    CipherEditor *editor() const { return m_editor; }
    AttachmentManager *attachments() const { return m_attachments; }

    // --- session ----------------------------------------------------------

    Q_INVOKABLE void login(const QString &email, const QString &password, const QString &serverUrl);
    /// Continue a login that was interrupted by a two-factor challenge.
    Q_INVOKABLE void submitTwoFactor(const QString &code, int provider, bool rememberDevice);
    /// Ask the server to email a two-step login code.
    Q_INVOKABLE void requestEmailCode();

    /// Continue a login the server paused because it does not recognize this device.
    Q_INVOKABLE void submitNewDeviceCode(const QString &code);
    /// Retry without a code, which makes the server email a fresh one.
    Q_INVOKABLE void resendNewDeviceCode();

    Q_INVOKABLE void cancelLogin();

    Q_INVOKABLE void unlock(const QString &password);
    Q_INVOKABLE void lock();
    /// Forget the account entirely, including the cached vault.
    Q_INVOKABLE void logout();

    Q_INVOKABLE void sync();

    /// Restart the inactivity timer; called from QML on user interaction.
    Q_INVOKABLE void noteActivity();

    // --- items ------------------------------------------------------------

    /// Every field of one item, for the detail view.
    Q_INVOKABLE QVariantMap cipherDetails(const QString &cipherId) const;

    /**
     * The one-time code for an item right now, or an empty string if it has
     * none. Computed on demand so list rows do not each need a running timer.
     */
    Q_INVOKABLE QString currentTotpCode(const QString &cipherId) const;

    Q_INVOKABLE void beginCreate(int cipherType, const QString &folderId = QString());
    Q_INVOKABLE bool beginEdit(const QString &cipherId);
    Q_INVOKABLE void saveEditor();

    Q_INVOKABLE void setFavorite(const QString &cipherId, bool favorite);
    Q_INVOKABLE void moveToTrash(const QString &cipherId);
    Q_INVOKABLE void restoreFromTrash(const QString &cipherId);
    Q_INVOKABLE void deleteForever(const QString &cipherId);

    Q_INVOKABLE void createFolder(const QString &name);
    Q_INVOKABLE void renameFolder(const QString &folderId, const QString &name);
    Q_INVOKABLE void deleteFolder(const QString &folderId);

Q_SIGNALS:
    void stateChanged();
    void accountChanged();
    void busyChanged();
    void syncingChanged();
    void offlineChanged();

    /// The server wants a second factor; @p providers holds TwoFactorProvider ids.
    void twoFactorRequired(const QList<int> &providers);
    /// The server emailed a code because it has not seen this device before.
    void newDeviceVerificationRequired(const QString &message);
    void loginFailed(const QString &message);
    void unlockFailed(const QString &message);
    void syncFinished(bool success, const QString &message);

    /// Non-fatal problems worth showing in a passive notification.
    void errorOccurred(const QString &message);
    void operationSucceeded(const QString &message);
    void cipherSaved(const QString &cipherId);

private:
    void setState(State state);
    void setBusy(bool busy);
    void setSyncing(bool syncing);
    void setOffline(bool offline);

    /**
     * Derive the master key on a worker thread.
     *
     * PBKDF2 at 600k iterations, and Argon2id even more so, take long enough to
     * freeze the window if run inline. @p then runs back on this thread.
     */
    void deriveMasterKeyAsync(const QString &email,
                              const SecureBytes &password,
                              const KdfConfig &kdf,
                              std::function<void(std::optional<SecureBytes>)> then);

    /**
     * Run @p then once the stored session has been read from the wallet,
     * reading it at most once. Deferred rather than done at startup: the wallet
     * is a D-Bus round trip that can stall for 25 seconds when nothing answers.
     */
    void withStoredSession(std::function<void()> then);
    /// The network half of sync(), once a session is known to be present.
    void performSync();

    void continueLogin();
    /**
     * Send the pending token request and interpret whatever the server asks
     * for next. Every login continuation funnels through here so the challenge
     * handling cannot drift between them.
     */
    void submitPendingToken();
    void completeLogin(const ApiResponse &response);
    void restoreSession();

    void applySyncPayload(const QJsonObject &payload);
    void refreshFolderCounts();
    void persistAccount();

    /// Push a changed item to the server and update the local model on success.
    void submitCipher(const Cipher &cipher, bool isNew);
    const Cipher *requireCipher(const QString &cipherId) const;

    void resetInactivityTimer();
    void clearSensitiveState();

    ApiClient *m_api;
    AppSettings *m_settings;
    TokenStore *m_tokens;
    LocalStore m_store;
    VaultKeys m_keys;

    CipherListModel *m_ciphers;
    CipherFilterProxyModel *m_filtered;
    FolderModel *m_folders;
    CipherEditor *m_editor;
    AttachmentManager *m_attachments;

    StoredAccount m_account;
    State m_state = LoggedOut;
    bool m_busy = false;
    bool m_syncing = false;
    bool m_offline = false;
    bool m_sessionRead = false;

    /// Held only between the password prompt and a successful token exchange.
    SecureBytes m_pendingPassword;
    SecureBytes m_pendingMasterKey;
    TokenRequest m_pendingToken;
    KdfConfig m_pendingKdf;

    QTimer m_inactivityTimer;
};

} // namespace kvault
