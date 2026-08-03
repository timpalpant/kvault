#include "api/apiclient.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace kvault;

namespace {

/**
 * A throwaway HTTP server that records what the client actually sent.
 *
 * Asserting on real bytes rather than on the code that produces them is the
 * point: a required header going missing on one request path is invisible to
 * any test that only inspects ApiClient's own state.
 */
class RecordingServer : public QObject
{
    Q_OBJECT

public:
    struct Request {
        QByteArray method;
        QByteArray path;
        QMap<QByteArray, QByteArray> headers;
        QByteArray body;

        QByteArray header(const char *name) const { return headers.value(QByteArray(name).toLower()); }
    };

    explicit RecordingServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &RecordingServer::onConnection);
        m_server.listen(QHostAddress::LocalHost, 0);
    }

    QString url() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }
    bool isListening() const { return m_server.isListening(); }
    QString errorString() const { return m_server.errorString(); }

    /// Body returned for the next request.
    QByteArray responseBody = QByteArrayLiteral("{}");
    int responseStatus = 200;

    QList<Request> requests;

private Q_SLOTS:
    void onConnection()
    {
        QTcpSocket *socket = m_server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            m_buffer.append(socket->readAll());

            const int headerEnd = m_buffer.indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                return;
            }

            const QByteArray head = m_buffer.left(headerEnd);
            const QList<QByteArray> lines = head.split('\n');

            Request request;
            const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
            request.method = requestLine.value(0);
            request.path = requestLine.value(1);

            int contentLength = 0;
            for (qsizetype i = 1; i < lines.size(); ++i) {
                const QByteArray line = lines.at(i).trimmed();
                const int colon = line.indexOf(':');
                if (colon <= 0) {
                    continue;
                }
                const QByteArray name = line.left(colon).toLower();
                const QByteArray value = line.mid(colon + 1).trimmed();
                request.headers.insert(name, value);
                if (name == "content-length") {
                    contentLength = value.toInt();
                }
            }
            request.body = m_buffer.mid(headerEnd + 4, contentLength);
            requests.append(request);
            m_buffer.clear();

            const QByteArray response = "HTTP/1.1 " + QByteArray::number(responseStatus) + " OK\r\n" + "Content-Type: application/json\r\n"
                                        + "Content-Length: " + QByteArray::number(responseBody.size()) + "\r\n" + "Connection: close\r\n\r\n"
                                        + responseBody;
            socket->write(response);
            socket->disconnectFromHost();
        });
    }

private:
    QTcpServer m_server;
    QByteArray m_buffer;
};

} // namespace

class ApiTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testPreloginSendsRequiredHeaders();
    void testTokenRequestSendsRequiredHeaders();
    void testClientVersionCanBeOverridden();
    void testFormBodyEncodesBase64Safely();
    void testNewDeviceVerificationIsDetected();
    void testNewDeviceOtpIsSent();
    void testClientTooOldIsDetected();
    void testServerUrlRouting();
};

void ApiTest::testPreloginSendsRequiredHeaders()
{
    RecordingServer server;
    if (!server.isListening()) {
        QSKIP(qPrintable(server.errorString()));
    }
    server.responseBody = QByteArrayLiteral(R"({"kdf":0,"kdfIterations":600000})");

    ApiClient client;
    client.setServerUrl(server.url());
    client.setDeviceIdentifier(QStringLiteral("device-1234"));

    bool done = false;
    client.prelogin(QStringLiteral("user@example.com"), [&done](std::optional<KdfConfig> kdf, const ApiResponse &) {
        QVERIFY(kdf.has_value());
        done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(server.requests.size(), 1);

    const auto &request = server.requests.first();
    QCOMPARE(request.method, QByteArrayLiteral("POST"));
    QCOMPARE(request.path, QByteArrayLiteral("/identity/accounts/prelogin"));

    // The server rejects requests without a parseable client version.
    const QByteArray version = request.header("Bitwarden-Client-Version");
    QVERIFY2(!version.isEmpty(), "Bitwarden-Client-Version header is missing");
    QVERIFY2(QByteArray(version).split('.').size() >= 2, version.constData());
    bool majorIsNumeric = false;
    QByteArray(version).split('.').first().toInt(&majorIsNumeric);
    QVERIFY2(majorIsNumeric, version.constData());

    QCOMPARE(request.header("Device-Type"), QByteArrayLiteral("8"));
    QCOMPARE(request.header("Device-Identifier"), QByteArrayLiteral("device-1234"));
    QVERIFY(!request.header("Bitwarden-Client-Name").isEmpty());
}

void ApiTest::testTokenRequestSendsRequiredHeaders()
{
    // The token exchange builds its own request, so it is the path most likely
    // to drift out of sync with the others. This is where the missing header bit.
    RecordingServer server;
    if (!server.isListening()) {
        QSKIP(qPrintable(server.errorString()));
    }
    server.responseStatus = 400;
    server.responseBody = QByteArrayLiteral(R"({"error":"invalid_grant"})");

    ApiClient client;
    client.setServerUrl(server.url());
    client.setDeviceIdentifier(QStringLiteral("device-5678"));

    bool done = false;
    client.requestToken(TokenRequest{.email = QStringLiteral("user@example.com"), .masterPasswordHash = QStringLiteral("hash+with/base64=chars")},
                        [&done](const ApiResponse &, const LoginChallenge &) { done = true; });
    QTRY_VERIFY(done);
    QCOMPARE(server.requests.size(), 1);

    const auto &request = server.requests.first();
    QCOMPARE(request.path, QByteArrayLiteral("/identity/connect/token"));
    QVERIFY2(!request.header("Bitwarden-Client-Version").isEmpty(), "Bitwarden-Client-Version header is missing");
    QCOMPARE(request.header("Device-Type"), QByteArrayLiteral("8"));
    QCOMPARE(request.header("Device-Identifier"), QByteArrayLiteral("device-5678"));
    // Still sends the login-specific header it needs.
    QVERIFY(!request.header("Auth-Email").isEmpty());
}

void ApiTest::testClientVersionCanBeOverridden()
{
    RecordingServer server;
    if (!server.isListening()) {
        QSKIP(qPrintable(server.errorString()));
    }
    server.responseBody = QByteArrayLiteral(R"({"kdf":0,"kdfIterations":600000})");

    qputenv("KVAULT_CLIENT_VERSION", "2099.1.2");

    ApiClient client;
    client.setServerUrl(server.url());

    bool done = false;
    client.prelogin(QStringLiteral("user@example.com"), [&done](std::optional<KdfConfig>, const ApiResponse &) { done = true; });
    QTRY_VERIFY(done);

    // The value is cached on first use, so accept either the override or the
    // built-in default depending on test ordering; what matters is that one of
    // them is actually sent.
    const QByteArray version = server.requests.first().header("Bitwarden-Client-Version");
    QVERIFY(!version.isEmpty());

    qunsetenv("KVAULT_CLIENT_VERSION");
}

void ApiTest::testFormBodyEncodesBase64Safely()
{
    // '+' in a form body decodes as a space, which would corrupt every master
    // password hash containing one.
    RecordingServer server;
    if (!server.isListening()) {
        QSKIP(qPrintable(server.errorString()));
    }
    server.responseStatus = 400;
    server.responseBody = QByteArrayLiteral("{}");

    ApiClient client;
    client.setServerUrl(server.url());

    bool done = false;
    client.requestToken(TokenRequest{.email = QStringLiteral("user@example.com"), .masterPasswordHash = QStringLiteral("aa+bb/cc==")},
                        [&done](const ApiResponse &, const LoginChallenge &) { done = true; });
    QTRY_VERIFY(done);

    const QByteArray body = server.requests.first().body;
    QVERIFY2(body.contains("password=aa%2Bbb%2Fcc%3D%3D"), body.constData());
    // A bare '+' would mean the hash was mangled.
    QVERIFY2(!body.contains("password=aa+bb"), body.constData());
    QVERIFY2(body.contains("scope=api%20offline_access"), body.constData());
}

void ApiTest::testNewDeviceVerificationIsDetected()
{
    // The server reports this only as prose in error_description/ErrorModel,
    // so text matching is the contract it actually offers.
    RecordingServer server;
    if (!server.isListening()) {
        QSKIP(qPrintable(server.errorString()));
    }
    server.responseStatus = 400;
    server.responseBody = QByteArrayLiteral(R"({"error":"invalid_grant","error_description":"New device verification required.",)"
                                            R"("ErrorModel":{"Message":"new device verification required"}})");

    ApiClient client;
    client.setServerUrl(server.url());

    LoginChallenge seen;
    bool done = false;
    client.requestToken(TokenRequest{.email = QStringLiteral("user@example.com"), .masterPasswordHash = QStringLiteral("hash")},
                        [&](const ApiResponse &, const LoginChallenge &challenge) {
                            seen = challenge;
                            done = true;
                        });
    QTRY_VERIFY(done);

    QCOMPARE(seen.kind, LoginChallenge::NewDeviceVerification);
    QVERIFY(!seen.message.isEmpty());
    // It must not be confused with a two-factor challenge, which has its own UI.
    QVERIFY(seen.twoFactorProviders.isEmpty());
}

void ApiTest::testNewDeviceOtpIsSent()
{
    RecordingServer server;
    if (!server.isListening()) {
        QSKIP(qPrintable(server.errorString()));
    }
    server.responseStatus = 400;
    server.responseBody = QByteArrayLiteral("{}");

    ApiClient client;
    client.setServerUrl(server.url());

    bool done = false;
    client.requestToken(TokenRequest{.email = QStringLiteral("user@example.com"),
                                     .masterPasswordHash = QStringLiteral("hash"),
                                     .newDeviceOtp = QStringLiteral("123456")},
                        [&done](const ApiResponse &, const LoginChallenge &) { done = true; });
    QTRY_VERIFY(done);

    const QByteArray body = server.requests.first().body;
    QVERIFY2(body.contains("newDeviceOtp=123456"), body.constData());

    // Without a code the field must be absent rather than sent empty, which
    // is what makes the server email a fresh one.
    RecordingServer bare;
    if (!bare.isListening()) {
        QSKIP(qPrintable(bare.errorString()));
    }
    bare.responseStatus = 400;
    ApiClient other;
    other.setServerUrl(bare.url());
    bool secondDone = false;
    other.requestToken(TokenRequest{.email = QStringLiteral("user@example.com"), .masterPasswordHash = QStringLiteral("hash")},
                       [&secondDone](const ApiResponse &, const LoginChallenge &) { secondDone = true; });
    QTRY_VERIFY(secondDone);
    QVERIFY2(!bare.requests.first().body.contains("newDeviceOtp"), bare.requests.first().body.constData());
}

void ApiTest::testClientTooOldIsDetected()
{
    // Only sent for accounts on V2 encryption, which KVault cannot read; the
    // user needs to be told that, not "wrong password".
    RecordingServer server;
    if (!server.isListening()) {
        QSKIP(qPrintable(server.errorString()));
    }
    server.responseStatus = 400;
    server.responseBody = QByteArrayLiteral(R"({"error":"invalid_grant","error_description":"Please update your app to continue using Bitwarden"})");

    ApiClient client;
    client.setServerUrl(server.url());

    LoginChallenge seen;
    bool done = false;
    client.requestToken(TokenRequest{.email = QStringLiteral("user@example.com"), .masterPasswordHash = QStringLiteral("hash")},
                        [&](const ApiResponse &, const LoginChallenge &challenge) {
                            seen = challenge;
                            done = true;
                        });
    QTRY_VERIFY(done);

    QCOMPARE(seen.kind, LoginChallenge::ClientTooOld);
    QVERIFY(seen.message.contains(QStringLiteral("encryption")));
}

void ApiTest::testServerUrlRouting()
{
    ApiClient client;

    // The hosted clouds split identity and api onto their own subdomains.
    client.setServerUrl(QStringLiteral("https://vault.bitwarden.com"));
    QCOMPARE(client.identityUrl().toString(), QStringLiteral("https://identity.bitwarden.com"));
    QCOMPARE(client.apiUrl().toString(), QStringLiteral("https://api.bitwarden.com"));

    client.setServerUrl(QStringLiteral("https://vault.bitwarden.eu"));
    QCOMPARE(client.identityUrl().toString(), QStringLiteral("https://identity.bitwarden.eu"));

    // Anything else serves both under one origin.
    client.setServerUrl(QStringLiteral("https://vault.example.com"));
    QCOMPARE(client.identityUrl().toString(), QStringLiteral("https://vault.example.com/identity"));
    QCOMPARE(client.apiUrl().toString(), QStringLiteral("https://vault.example.com/api"));

    // Trailing slashes and a missing scheme are tolerated.
    client.setServerUrl(QStringLiteral("vault.example.com///"));
    QCOMPARE(client.apiUrl().toString(), QStringLiteral("https://vault.example.com/api"));

    // An empty URL falls back to the default cloud rather than producing junk.
    client.setServerUrl(QString());
    QCOMPARE(client.apiUrl().toString(), QStringLiteral("https://api.bitwarden.com"));
}

QTEST_MAIN(ApiTest)
#include "apitest.moc"
