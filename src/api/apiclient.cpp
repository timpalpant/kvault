#include "apiclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

Q_LOGGING_CATEGORY(KVAULT_API, "kvault.api")

namespace kvault {

namespace {

/// DeviceType.LinuxDesktop in the server's enum.
constexpr int LinuxDesktopDeviceType = 8;
constexpr const char *ClientId = "desktop";
constexpr const char *UserAgent = "kvault/0.1 (Linux)";

/**
 * The version reported in the Bitwarden-Client-Version header.
 *
 * The server rejects requests without it ("No client version header found,
 * required to prevent encryption errors") and uses it to decide which
 * ciphertext formats it may hand back. KVault implements the classic EncString
 * scheme (AES-CBC + HMAC-SHA256), not the newer COSE/XChaCha20-Poly1305 format,
 * so this deliberately claims a release from before that rollout rather than
 * the latest: claiming to be newer than we are risks being served items we
 * cannot decrypt.
 *
 * Override with the KVAULT_CLIENT_VERSION environment variable if a server
 * rejects this as too old.
 */
constexpr const char *DefaultClientVersion = "2025.9.0";

QByteArray clientVersion()
{
    static const QByteArray version = []() -> QByteArray {
        const QByteArray fromEnvironment = qgetenv("KVAULT_CLIENT_VERSION").trimmed();
        return fromEnvironment.isEmpty() ? QByteArray(DefaultClientVersion) : fromEnvironment;
    }();
    return version;
}

/// Refresh this long before the token actually expires.
constexpr int TokenRefreshMarginSeconds = 60;

QByteArray jsonBody(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

/**
 * Encode an application/x-www-form-urlencoded body.
 *
 * QUrlQuery is not usable here: it leaves '+' unencoded, and a form body is
 * decoded with '+' meaning space. Master password hashes are base64 and
 * routinely contain '+', so that would corrupt them.
 */
QByteArray formBody(const QList<QPair<QString, QString>> &fields)
{
    QByteArray body;
    for (const auto &[key, value] : fields) {
        if (!body.isEmpty()) {
            body.append('&');
        }
        body.append(QUrl::toPercentEncoding(key));
        body.append('=');
        body.append(QUrl::toPercentEncoding(value));
    }
    return body;
}

/// Pull whatever the server offered as an explanation out of an error body.
QString extractErrorMessage(const QJsonObject &json, int httpStatus)
{
    const auto firstNonEmpty = [&json](std::initializer_list<const char *> keys) -> QString {
        for (const char *key : keys) {
            const QString value = json.value(QLatin1String(key)).toString();
            if (!value.isEmpty()) {
                return value;
            }
        }
        return {};
    };

    // Validation errors are the most specific thing available, so prefer them.
    for (const char *key : {"validationErrors", "ValidationErrors"}) {
        const QJsonObject errors = json.value(QLatin1String(key)).toObject();
        for (auto it = errors.begin(); it != errors.end(); ++it) {
            const QJsonArray messages = it.value().toArray();
            if (!messages.isEmpty()) {
                return messages.first().toString();
            }
        }
    }

    const QString message = firstNonEmpty({"error_description", "message", "Message", "error"});
    if (!message.isEmpty()) {
        return message;
    }

    switch (httpStatus) {
    case 401:
        return QObject::tr("Not authorised. Your session may have expired.");
    case 403:
        return QObject::tr("The server refused this request.");
    case 404:
        return QObject::tr("Not found on the server.");
    case 429:
        return QObject::tr("Too many requests. Wait a moment and try again.");
    default:
        break;
    }
    return QObject::tr("The server returned an error (HTTP %1).").arg(httpStatus);
}

} // namespace

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setAutoDeleteReplies(true);
    setServerUrl(QStringLiteral("https://vault.bitwarden.com"));
}

ApiClient::~ApiClient() = default;

void ApiClient::setServerUrl(const QString &serverUrl)
{
    QString normalised = serverUrl.trimmed();
    while (normalised.endsWith(u'/')) {
        normalised.chop(1);
    }
    if (normalised.isEmpty()) {
        normalised = QStringLiteral("https://vault.bitwarden.com");
    }
    if (!normalised.contains(QStringLiteral("://"))) {
        normalised.prepend(QStringLiteral("https://"));
    }

    m_serverUrl = normalised;

    const QUrl url(normalised);
    const QString host = url.host();

    // The hosted clouds put identity and api on their own subdomains; a
    // self-hosted instance serves both under one origin.
    QString officialDomain;
    for (const QLatin1String candidate : {QLatin1String("bitwarden.com"), QLatin1String("bitwarden.eu")}) {
        if (host == candidate || host.endsWith(QLatin1Char('.') + candidate)) {
            officialDomain = candidate;
            break;
        }
    }

    if (!officialDomain.isEmpty()) {
        m_identityUrl = QUrl(QStringLiteral("https://identity.") + officialDomain);
        m_apiUrl = QUrl(QStringLiteral("https://api.") + officialDomain);
    } else {
        m_identityUrl = QUrl(normalised + QStringLiteral("/identity"));
        m_apiUrl = QUrl(normalised + QStringLiteral("/api"));
    }

    qCInfo(KVAULT_API) << "Server set to" << m_serverUrl << "identity:" << m_identityUrl.toString() << "api:" << m_apiUrl.toString();
}

void ApiClient::setTokens(const QString &accessToken, const QString &refreshToken, const QDateTime &expiry)
{
    m_accessToken = accessToken;
    m_refreshToken = refreshToken;
    m_tokenExpiry = expiry;
}

void ApiClient::clearTokens()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_tokenExpiry = QDateTime();
    m_pendingAfterRefresh.clear();
    m_refreshInFlight = false;
}

QUrl ApiClient::apiEndpoint(const QString &path) const
{
    return QUrl(m_apiUrl.toString() + path);
}

void ApiClient::applyCommonHeaders(QNetworkRequest &request) const
{
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(UserAgent));

    // The server refuses requests without a parseable client version, and reads
    // the device headers to recognise this installation.
    request.setRawHeader("Bitwarden-Client-Version", clientVersion());
    request.setRawHeader("Bitwarden-Client-Name", ClientId);
    request.setRawHeader("Is-Prerelease", "0");
    request.setRawHeader("Device-Type", QByteArray::number(LinuxDesktopDeviceType));
    if (!m_deviceIdentifier.isEmpty()) {
        request.setRawHeader("Device-Identifier", m_deviceIdentifier.toUtf8());
    }
}

void ApiClient::sendRequest(
    Method method, const QUrl &url, const QByteArray &body, const QByteArray &contentType, bool authenticated, Handler handler)
{
    QNetworkRequest request(url);
    applyCommonHeaders(request);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    if (!contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }
    if (authenticated && !m_accessToken.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    }

    QNetworkReply *reply = nullptr;
    switch (method) {
    case Method::Get:
        reply = m_network->get(request);
        break;
    case Method::Post:
        reply = m_network->post(request, body);
        break;
    case Method::Put:
        reply = m_network->put(request, body);
        break;
    case Method::Delete:
        reply = body.isEmpty() ? m_network->deleteResource(request) : m_network->sendCustomRequest(request, "DELETE", body);
        break;
    }

    connect(reply, &QNetworkReply::finished, this, [reply, handler = std::move(handler)]() {
        ApiResponse response;
        response.raw = reply->readAll();
        response.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (!response.raw.isEmpty()) {
            const QJsonDocument document = QJsonDocument::fromJson(response.raw);
            if (document.isObject()) {
                response.json = document.object();
            }
        }

        if (reply->error() != QNetworkReply::NoError && response.httpStatus == 0) {
            response.ok = false;
            response.networkError = true;
            response.errorMessage = reply->errorString();
            qCWarning(KVAULT_API) << "Network error:" << response.errorMessage;
        } else if (response.httpStatus >= 200 && response.httpStatus < 300) {
            response.ok = true;
        } else {
            response.ok = false;
            response.errorMessage = extractErrorMessage(response.json, response.httpStatus);
            qCWarning(KVAULT_API) << "HTTP" << response.httpStatus << response.errorMessage;
        }

        if (handler) {
            handler(response);
        }
    });
}

void ApiClient::withValidToken(std::function<void(bool)> work)
{
    const bool expiringSoon = m_tokenExpiry.isValid() && QDateTime::currentDateTimeUtc().secsTo(m_tokenExpiry) < TokenRefreshMarginSeconds;

    if (!m_accessToken.isEmpty() && !expiringSoon) {
        work(true);
        return;
    }
    if (m_refreshToken.isEmpty()) {
        work(!m_accessToken.isEmpty());
        return;
    }

    m_pendingAfterRefresh.append(std::move(work));
    if (!m_refreshInFlight) {
        refreshTokens();
    }
}

void ApiClient::authenticatedRequest(Method method, const QString &path, const QByteArray &body, const QByteArray &contentType, Handler handler)
{
    withValidToken([this, method, path, body, contentType, handler = std::move(handler)](bool authorised) {
        if (!authorised) {
            ApiResponse response;
            response.errorMessage = QObject::tr("Your session has expired. Please log in again.");
            if (handler) {
                handler(response);
            }
            return;
        }
        sendRequest(method, apiEndpoint(path), body, contentType, true, handler);
    });
}

void ApiClient::refreshTokens()
{
    m_refreshInFlight = true;

    const QByteArray body = formBody({
        {QStringLiteral("grant_type"), QStringLiteral("refresh_token")},
        {QStringLiteral("refresh_token"), m_refreshToken},
        {QStringLiteral("client_id"), QString::fromLatin1(ClientId)},
    });

    sendRequest(Method::Post,
                QUrl(m_identityUrl.toString() + QStringLiteral("/connect/token")),
                body,
                "application/x-www-form-urlencoded",
                false,
                [this](const ApiResponse &response) {
                    m_refreshInFlight = false;
                    const bool success = response.ok && response.json.contains(QStringLiteral("access_token"));
                    if (success) {
                        handleTokenResponse(response);
                        Q_EMIT tokensRefreshed();
                    } else if (!response.networkError) {
                        // The refresh token itself was rejected: the session is gone.
                        qCWarning(KVAULT_API) << "Refresh token rejected, session expired";
                        clearTokens();
                        Q_EMIT sessionExpired();
                    }

                    const auto pending = std::exchange(m_pendingAfterRefresh, {});
                    for (const auto &work : pending) {
                        work(success);
                    }
                });
}

void ApiClient::handleTokenResponse(const ApiResponse &response)
{
    m_accessToken = response.json.value(QStringLiteral("access_token")).toString();
    const QString refresh = response.json.value(QStringLiteral("refresh_token")).toString();
    if (!refresh.isEmpty()) {
        m_refreshToken = refresh;
    }
    const int expiresIn = response.json.value(QStringLiteral("expires_in")).toInt(3600);
    m_tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
}

void ApiClient::prelogin(const QString &email, std::function<void(std::optional<KdfConfig>, const ApiResponse &)> handler)
{
    const QJsonObject body{{QStringLiteral("email"), AccountCrypto::normaliseEmail(email)}};

    sendRequest(Method::Post,
                QUrl(m_identityUrl.toString() + QStringLiteral("/accounts/prelogin")),
                jsonBody(body),
                "application/json",
                false,
                [handler = std::move(handler)](const ApiResponse &response) {
                    if (!response.ok) {
                        handler(std::nullopt, response);
                        return;
                    }
                    const KdfConfig kdf = KdfConfig::fromJson(response.json);
                    if (!kdf.isValid()) {
                        ApiResponse failed = response;
                        failed.ok = false;
                        failed.errorMessage = QObject::tr("The server reported unusable password settings.");
                        handler(std::nullopt, failed);
                        return;
                    }
                    handler(kdf, response);
                });
}

void ApiClient::requestToken(const TokenRequest &tokenRequest, std::function<void(const ApiResponse &, const LoginChallenge &)> handler)
{
    const QString normalisedEmail = AccountCrypto::normaliseEmail(tokenRequest.email);

    QList<QPair<QString, QString>> fields{
        {QStringLiteral("grant_type"), QStringLiteral("password")},
        {QStringLiteral("username"), normalisedEmail},
        {QStringLiteral("password"), tokenRequest.masterPasswordHash},
        {QStringLiteral("scope"), QStringLiteral("api offline_access")},
        {QStringLiteral("client_id"), QString::fromLatin1(ClientId)},
        {QStringLiteral("deviceType"), QString::number(LinuxDesktopDeviceType)},
        {QStringLiteral("deviceIdentifier"), m_deviceIdentifier},
        {QStringLiteral("deviceName"), QStringLiteral("linux")},
    };

    if (!tokenRequest.twoFactorCode.isEmpty()) {
        fields.append({QStringLiteral("twoFactorToken"), tokenRequest.twoFactorCode});
        fields.append({QStringLiteral("twoFactorProvider"), QString::number(int(tokenRequest.twoFactorProvider))});
        fields.append({QStringLiteral("twoFactorRemember"), tokenRequest.rememberDevice ? QStringLiteral("1") : QStringLiteral("0")});
    }

    // The server emails this code the first time it sees a device, then expects
    // the same token request replayed with the code attached.
    if (!tokenRequest.newDeviceOtp.isEmpty()) {
        fields.append({QStringLiteral("newDeviceOtp"), tokenRequest.newDeviceOtp});
    }

    QNetworkRequest request(QUrl(m_identityUrl.toString() + QStringLiteral("/connect/token")));
    applyCommonHeaders(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    // The identity server wants the email echoed in a header for rate limiting.
    request.setRawHeader("Auth-Email", normalisedEmail.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    QNetworkReply *reply = m_network->post(request, formBody(fields));

    connect(reply, &QNetworkReply::finished, this, [this, reply, handler = std::move(handler)]() {
        ApiResponse response;
        response.raw = reply->readAll();
        response.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonDocument document = QJsonDocument::fromJson(response.raw);
        if (document.isObject()) {
            response.json = document.object();
        }

        LoginChallenge challenge;

        if (reply->error() != QNetworkReply::NoError && response.httpStatus == 0) {
            response.networkError = true;
            response.errorMessage = reply->errorString();
            handler(response, challenge);
            return;
        }

        if (response.httpStatus >= 200 && response.httpStatus < 300 && response.json.contains(QStringLiteral("access_token"))) {
            response.ok = true;
            handleTokenResponse(response);
            handler(response, challenge);
            return;
        }

        response.ok = false;
        response.errorMessage = extractErrorMessage(response.json, response.httpStatus);

        // A 400 here usually means "more input needed" rather than a hard failure.
        const QJsonObject providers = response.json.value(QStringLiteral("TwoFactorProviders2")).toObject();
        if (!providers.isEmpty()) {
            challenge.kind = LoginChallenge::TwoFactorRequired;
            for (auto it = providers.begin(); it != providers.end(); ++it) {
                bool ok = false;
                const int id = it.key().toInt(&ok);
                if (ok) {
                    challenge.twoFactorProviders.append(id);
                }
            }
            std::sort(challenge.twoFactorProviders.begin(), challenge.twoFactorProviders.end());
        } else if (const QJsonArray legacy = response.json.value(QStringLiteral("TwoFactorProviders")).toArray(); !legacy.isEmpty()) {
            challenge.kind = LoginChallenge::TwoFactorRequired;
            for (const QJsonValue &value : legacy) {
                challenge.twoFactorProviders.append(value.toString().toInt());
            }
        }

        const QString captchaKey = response.json.value(QStringLiteral("HCaptcha_SiteKey")).toString();
        if (!captchaKey.isEmpty() && challenge.kind == LoginChallenge::None) {
            challenge.kind = LoginChallenge::CaptchaRequired;
            challenge.captchaSiteKey = captchaKey;
            challenge.message = QObject::tr("This server is asking for a captcha, which this app cannot show. "
                                            "Log in once via the web vault, then try again.");
        }

        if (challenge.kind == LoginChallenge::None) {
            // These two are reported only as prose, so matching on the text is
            // the contract the server actually offers.
            const QString detail = (response.errorMessage + QLatin1Char(' ')
                                    + response.json.value(QStringLiteral("ErrorModel")).toObject().value(QStringLiteral("Message")).toString())
                                       .toLower();

            if (detail.contains(QLatin1String("new device verification"))) {
                challenge.kind = LoginChallenge::NewDeviceVerification;
                challenge.message = QObject::tr("This device is new to your account. Check your email for a verification code.");
            } else if (detail.contains(QLatin1String("update your app"))) {
                // The server only enforces a minimum client version for accounts
                // using V2 encryption, which KVault cannot read anyway.
                challenge.kind = LoginChallenge::ClientTooOld;
                challenge.message = QObject::tr("This account uses a newer encryption format that KVault does not support yet. "
                                                "The server is asking for a newer client.");
            }
        }

        handler(response, challenge);
    });
}

void ApiClient::sendEmailLoginCode(const QString &email, const QString &masterPasswordHash, Handler handler)
{
    const QJsonObject body{
        {QStringLiteral("email"), AccountCrypto::normaliseEmail(email)},
        {QStringLiteral("masterPasswordHash"), masterPasswordHash},
        {QStringLiteral("deviceIdentifier"), m_deviceIdentifier},
    };
    sendRequest(
        Method::Post, apiEndpoint(QStringLiteral("/two-factor/send-email-login")), jsonBody(body), "application/json", false, std::move(handler));
}

void ApiClient::sync(Handler handler)
{
    authenticatedRequest(Method::Get, QStringLiteral("/sync?excludeDomains=true"), {}, {}, std::move(handler));
}

void ApiClient::createCipher(const QJsonObject &cipher, Handler handler)
{
    authenticatedRequest(Method::Post, QStringLiteral("/ciphers"), jsonBody(cipher), "application/json", std::move(handler));
}

void ApiClient::updateCipher(const QString &cipherId, const QJsonObject &cipher, Handler handler)
{
    authenticatedRequest(Method::Put, QStringLiteral("/ciphers/") + cipherId, jsonBody(cipher), "application/json", std::move(handler));
}

void ApiClient::softDeleteCipher(const QString &cipherId, Handler handler)
{
    authenticatedRequest(Method::Put, QStringLiteral("/ciphers/") + cipherId + QStringLiteral("/delete"), {}, "application/json", std::move(handler));
}

void ApiClient::restoreCipher(const QString &cipherId, Handler handler)
{
    authenticatedRequest(
        Method::Put, QStringLiteral("/ciphers/") + cipherId + QStringLiteral("/restore"), {}, "application/json", std::move(handler));
}

void ApiClient::deleteCipher(const QString &cipherId, Handler handler)
{
    authenticatedRequest(Method::Delete, QStringLiteral("/ciphers/") + cipherId, {}, {}, std::move(handler));
}

void ApiClient::createFolder(const QJsonObject &folder, Handler handler)
{
    authenticatedRequest(Method::Post, QStringLiteral("/folders"), jsonBody(folder), "application/json", std::move(handler));
}

void ApiClient::updateFolder(const QString &folderId, const QJsonObject &folder, Handler handler)
{
    authenticatedRequest(Method::Put, QStringLiteral("/folders/") + folderId, jsonBody(folder), "application/json", std::move(handler));
}

void ApiClient::deleteFolder(const QString &folderId, Handler handler)
{
    authenticatedRequest(Method::Delete, QStringLiteral("/folders/") + folderId, {}, {}, std::move(handler));
}

void ApiClient::attachmentInfo(const QString &cipherId, const QString &attachmentId, Handler handler)
{
    authenticatedRequest(
        Method::Get, QStringLiteral("/ciphers/") + cipherId + QStringLiteral("/attachment/") + attachmentId, {}, {}, std::move(handler));
}

void ApiClient::downloadUrl(const QString &url, Handler handler)
{
    // Attachment blobs live on object storage with a pre-signed URL, so this
    // deliberately does not send the Bitwarden bearer token.
    sendRequest(Method::Get, QUrl(url), {}, {}, false, std::move(handler));
}

} // namespace kvault
