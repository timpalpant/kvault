#pragma once

#include "crypto/kdf.h"

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

namespace kvault {

/// The outcome of a single HTTP call.
struct ApiResponse {
    bool ok = false;
    int httpStatus = 0;
    QJsonObject json;
    QByteArray raw;
    /// Human-readable message suitable for showing in the UI.
    QString errorMessage;
    /// True when the failure was a transport problem rather than a server rejection.
    bool networkError = false;
};

/// Two-factor provider ids as defined by the server.
enum class TwoFactorProvider {
    Authenticator = 0,
    Email = 1,
    Duo = 2,
    YubiKey = 3,
    U2f = 4,
    Remember = 5,
    OrganizationDuo = 6,
    WebAuthn = 7,
};

/// What the identity server said when a token request could not be completed.
struct LoginChallenge {
    enum Kind {
        None,
        TwoFactorRequired,
        CaptchaRequired,
        /// The server emailed a one-time code because it does not know this device.
        NewDeviceVerification,
        /// The account needs a client newer than this one, i.e. V2 encryption.
        ClientTooOld,
    };
    Kind kind = None;
    /// Provider id -> provider-specific metadata, from TwoFactorProviders2.
    QList<int> twoFactorProviders;
    QString captchaSiteKey;
    QString message;
};

/// Everything the identity server needs for the password grant.
struct TokenRequest {
    QString email;
    QString masterPasswordHash;

    /// Second-factor code, once the server has asked for one.
    QString twoFactorCode;
    TwoFactorProvider twoFactorProvider = TwoFactorProvider::Authenticator;
    bool rememberDevice = false;

    /// The emailed code for a device the server has not seen before.
    QString newDeviceOtp;
};

/**
 * Talks to a Bitwarden server.
 *
 * Knows about tokens and refreshes them transparently, but knows nothing about
 * encryption: everything it returns is still ciphertext.
 */
class ApiClient : public QObject
{
    Q_OBJECT

public:
    using Handler = std::function<void(const ApiResponse &)>;

    explicit ApiClient(QObject *parent = nullptr);
    ~ApiClient() override;

    /**
     * Point the client at a deployment.
     *
     * The official clouds split identity and api onto separate hosts; any other
     * URL is treated as a self-hosted instance where both live under one origin.
     */
    void setServerUrl(const QString &serverUrl);
    QString serverUrl() const { return m_serverUrl; }
    QUrl identityUrl() const { return m_identityUrl; }
    QUrl apiUrl() const { return m_apiUrl; }

    void setDeviceIdentifier(const QString &identifier) { m_deviceIdentifier = identifier; }

    /// Restore a previous session without going through the password flow again.
    void setTokens(const QString &accessToken, const QString &refreshToken, const QDateTime &expiry);
    QString accessToken() const { return m_accessToken; }
    QString refreshToken() const { return m_refreshToken; }
    QDateTime tokenExpiry() const { return m_tokenExpiry; }
    bool hasTokens() const { return !m_accessToken.isEmpty(); }
    void clearTokens();

    // --- authentication ---------------------------------------------------

    /// Fetch the account's KDF parameters. Works without authentication.
    void prelogin(const QString &email, std::function<void(std::optional<KdfConfig>, const ApiResponse &)> handler);

    /**
     * Exchange the master password hash for tokens.
     *
     * The same call is repeated as the server asks for more: first bare, then
     * with a two-factor code, or with the emailed new-device code.
     */
    void requestToken(const TokenRequest &request, std::function<void(const ApiResponse &, const LoginChallenge &)> handler);

    /// Ask the server to email a login code.
    void sendEmailLoginCode(const QString &email, const QString &masterPasswordHash, Handler handler);

    // --- vault ------------------------------------------------------------

    void sync(Handler handler);

    void createCipher(const QJsonObject &cipher, Handler handler);
    void updateCipher(const QString &cipherId, const QJsonObject &cipher, Handler handler);
    /// Move to trash.
    void softDeleteCipher(const QString &cipherId, Handler handler);
    void restoreCipher(const QString &cipherId, Handler handler);
    /// Delete permanently.
    void deleteCipher(const QString &cipherId, Handler handler);

    void createFolder(const QJsonObject &folder, Handler handler);
    void updateFolder(const QString &folderId, const QJsonObject &folder, Handler handler);
    void deleteFolder(const QString &folderId, Handler handler);

    /// Get fresh attachment metadata, including a download URL that has not expired.
    void attachmentInfo(const QString &cipherId, const QString &attachmentId, Handler handler);
    /// Download raw (still encrypted) attachment bytes from a storage URL.
    void downloadUrl(const QString &url, Handler handler);

Q_SIGNALS:
    /// Emitted when the refresh token is no longer accepted and the user must log in again.
    void sessionExpired();
    void tokensRefreshed();

private:
    enum class Method { Get, Post, Put, Delete };

    /**
     * Apply the headers every request needs, including the client version the
     * server requires. Both the token exchange and ordinary API calls go
     * through this so neither can drift out of sync.
     */
    void applyCommonHeaders(QNetworkRequest &request) const;
    void sendRequest(Method method, const QUrl &url, const QByteArray &body, const QByteArray &contentType, bool authenticated, Handler handler);
    /// Send a request against the api host, refreshing the token first if needed.
    void authenticatedRequest(Method method, const QString &path, const QByteArray &body, const QByteArray &contentType, Handler handler);
    /// Run @p work once a valid access token is available.
    void withValidToken(std::function<void(bool)> work);
    void refreshTokens();
    void handleTokenResponse(const ApiResponse &response);

    QUrl apiEndpoint(const QString &path) const;

    QNetworkAccessManager *m_network;
    QString m_serverUrl;
    QUrl m_identityUrl;
    QUrl m_apiUrl;
    QString m_deviceIdentifier;

    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_tokenExpiry;

    bool m_refreshInFlight = false;
    QList<std::function<void(bool)>> m_pendingAfterRefresh;
};

} // namespace kvault
