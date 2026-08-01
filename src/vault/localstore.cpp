#include "localstore.h"

#include <qt6keychain/keychain.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

Q_DECLARE_LOGGING_CATEGORY(KVAULT_VAULT)

namespace kvault {

namespace {
constexpr const char *KeychainService = "io.github.timpalpant.kvault";
constexpr const char *KeychainKey = "session-tokens";
} // namespace

bool StoredAccount::isValid() const
{
    return !email.isEmpty() && !serverUrl.isEmpty() && !wrappedUserKey.isEmpty() && kdf.isValid();
}

QString LocalStore::dataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString LocalStore::accountPath() const
{
    return dataDirectory() + QStringLiteral("/account.json");
}

QString LocalStore::vaultPath() const
{
    return dataDirectory() + QStringLiteral("/vault.json");
}

bool LocalStore::writeJson(const QString &path, const QJsonObject &json)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(KVAULT_VAULT) << "Cannot write" << path << file.errorString();
        return false;
    }
    // Restrict before writing so the contents are never briefly world-readable.
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qCWarning(KVAULT_VAULT) << "Cannot commit" << path << file.errorString();
        return false;
    }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

std::optional<QJsonObject> LocalStore::readJson(const QString &path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        qCWarning(KVAULT_VAULT) << "Corrupt cache file" << path;
        return std::nullopt;
    }
    return document.object();
}

bool LocalStore::saveAccount(const StoredAccount &account) const
{
    QJsonObject json{
        {QStringLiteral("email"), account.email},
        {QStringLiteral("serverUrl"), account.serverUrl},
        {QStringLiteral("deviceIdentifier"), account.deviceIdentifier},
        {QStringLiteral("wrappedUserKey"), account.wrappedUserKey},
        {QStringLiteral("wrappedPrivateKey"), account.wrappedPrivateKey},
        {QStringLiteral("kdf"), account.kdf.toJson()},
    };
    if (account.lastSync.isValid()) {
        json.insert(QStringLiteral("lastSync"), account.lastSync.toUTC().toString(Qt::ISODate));
    }
    return writeJson(accountPath(), json);
}

std::optional<StoredAccount> LocalStore::loadAccount() const
{
    const auto json = readJson(accountPath());
    if (!json) {
        return std::nullopt;
    }

    StoredAccount account;
    account.email = json->value(QStringLiteral("email")).toString();
    account.serverUrl = json->value(QStringLiteral("serverUrl")).toString();
    account.deviceIdentifier = json->value(QStringLiteral("deviceIdentifier")).toString();
    account.wrappedUserKey = json->value(QStringLiteral("wrappedUserKey")).toString();
    account.wrappedPrivateKey = json->value(QStringLiteral("wrappedPrivateKey")).toString();
    account.kdf = KdfConfig::fromJson(json->value(QStringLiteral("kdf")).toObject());
    account.lastSync = QDateTime::fromString(json->value(QStringLiteral("lastSync")).toString(), Qt::ISODate);

    if (!account.isValid()) {
        qCWarning(KVAULT_VAULT) << "Stored account is incomplete, ignoring it";
        return std::nullopt;
    }
    return account;
}

bool LocalStore::saveSyncPayload(const QJsonObject &payload) const
{
    return writeJson(vaultPath(), payload);
}

std::optional<QJsonObject> LocalStore::loadSyncPayload() const
{
    return readJson(vaultPath());
}

bool LocalStore::hasCachedVault() const
{
    return QFile::exists(vaultPath()) && QFile::exists(accountPath());
}

void LocalStore::clear() const
{
    QFile::remove(accountPath());
    QFile::remove(vaultPath());
}

// ---------------------------------------------------------------------------

TokenStore::TokenStore(QObject *parent)
    : QObject(parent)
{}

void TokenStore::save(const QString &email, const QString &accessToken, const QString &refreshToken, const QDateTime &expiry)
{
    const QJsonObject json{
        {QStringLiteral("email"), email},
        {QStringLiteral("accessToken"), accessToken},
        {QStringLiteral("refreshToken"), refreshToken},
        {QStringLiteral("expiry"), expiry.toUTC().toString(Qt::ISODate)},
    };

    auto *job = new QKeychain::WritePasswordJob(QLatin1String(KeychainService), this);
    job->setKey(QLatin1String(KeychainKey));
    job->setTextData(QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact)));
    job->setAutoDelete(true);
    connect(job, &QKeychain::Job::finished, this, [](QKeychain::Job *finished) {
        if (finished->error() != QKeychain::NoError) {
            // Not fatal: the user just has to type their password again next launch.
            qCWarning(KVAULT_VAULT) << "Could not store session tokens:" << finished->errorString();
        }
    });
    job->start();
}

void TokenStore::load(std::function<void(const StoredTokens &)> handler)
{
    auto *job = new QKeychain::ReadPasswordJob(QLatin1String(KeychainService), this);
    job->setKey(QLatin1String(KeychainKey));
    job->setAutoDelete(true);
    connect(job, &QKeychain::Job::finished, this, [handler = std::move(handler)](QKeychain::Job *finished) {
        auto *read = static_cast<QKeychain::ReadPasswordJob *>(finished);
        StoredTokens tokens;

        if (read->error() != QKeychain::NoError) {
            qCInfo(KVAULT_VAULT) << "No stored session:" << read->errorString();
            handler(tokens);
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(read->textData().toUtf8());
        if (!document.isObject()) {
            qCWarning(KVAULT_VAULT) << "Stored session is unreadable";
            handler(tokens);
            return;
        }

        const QJsonObject json = document.object();
        tokens.found = true;
        tokens.email = json.value(QStringLiteral("email")).toString();
        tokens.accessToken = json.value(QStringLiteral("accessToken")).toString();
        tokens.refreshToken = json.value(QStringLiteral("refreshToken")).toString();
        tokens.expiry = QDateTime::fromString(json.value(QStringLiteral("expiry")).toString(), Qt::ISODate);
        handler(tokens);
    });
    job->start();
}

void TokenStore::clear()
{
    auto *job = new QKeychain::DeletePasswordJob(QLatin1String(KeychainService), this);
    job->setKey(QLatin1String(KeychainKey));
    job->setAutoDelete(true);
    job->start();
}

} // namespace kvault
