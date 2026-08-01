#include "app/appsettings.h"
#include "crypto/encstring.h"
#include "crypto/kdf.h"
#include "model/cipher.h"
#include "vault/cipherfilterproxymodel.h"
#include "vault/cipherlistmodel.h"
#include "vault/foldermodel.h"
#include "vault/localstore.h"
#include "vault/vaultmanager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace kvault;

namespace {

constexpr const char *TestEmail = "user@example.com";
constexpr const char *TestPassword = "correct horse battery staple";
/// Deliberately low so the test does not spend a second on key derivation.
constexpr int TestIterations = 1000;

} // namespace

/**
 * Drives VaultManager against a synthetic on-disk vault.
 *
 * This covers the path a user actually takes on a cold start: the stored
 * account is read, the master password unwraps the user key, and the cached
 * sync payload is decrypted into the models. No network is involved.
 */
class VaultTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    void testStartsLockedWithStoredAccount();
    void testUnlockDecryptsCachedVault();
    void testWrongPasswordIsRejected();
    void testLockClearsDecryptedItems();
    void testFilteringAndSearch();
    void testEditorRoundTrip();
    void testLogoutRemovesCachedVault();

private:
    /// Write a synthetic account and encrypted vault into the test data dir.
    void writeSyntheticVault();
    /// Unlocking derives the key on a worker thread, so tests have to wait.
    void unlockAndWait(VaultManager &manager);

    SymmetricKey m_userKey;
    QString m_folderId;
};

void VaultTest::initTestCase()
{
    // Redirects QStandardPaths and QSettings into a throwaway location, so the
    // test can never touch a real vault.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("io.github.timpalpant"));
    QCoreApplication::setApplicationName(QStringLiteral("kvault-vaulttest"));
}

void VaultTest::init()
{
    LocalStore().clear();
    QSettings settings;
    settings.clear();
    // Unlocking must not try to reach the network during the test.
    settings.setValue(QStringLiteral("sync/onUnlock"), false);
    settings.sync();

    writeSyntheticVault();
}

void VaultTest::writeSyntheticVault()
{
    KdfConfig kdf;
    kdf.type = KdfType::Pbkdf2Sha256;
    kdf.iterations = TestIterations;

    const auto masterKey = AccountCrypto::deriveMasterKey(QString::fromLatin1(TestEmail), SecureBytes(QByteArray(TestPassword)), kdf);
    QVERIFY(masterKey.has_value());
    const SymmetricKey stretched = SymmetricKey::stretch(*masterKey);

    m_userKey = SymmetricKey::generate();
    const QString wrappedUserKey = EncString::encrypt(m_userKey.full(), stretched).toString();

    StoredAccount account;
    account.email = QString::fromLatin1(TestEmail);
    // A closed local port: the suite must never be able to reach a real server.
    account.serverUrl = QStringLiteral("http://127.0.0.1:1");
    account.deviceIdentifier = QStringLiteral("11111111-2222-3333-4444-555555555555");
    account.wrappedUserKey = wrappedUserKey;
    account.kdf = kdf;
    account.lastSync = QDateTime::currentDateTimeUtc();

    LocalStore store;
    QVERIFY(store.saveAccount(account));

    // Build a sync payload shaped like the server's, with real ciphertext.
    m_folderId = QStringLiteral("f0000000-0000-0000-0000-000000000001");
    QJsonObject folder{
        {QStringLiteral("id"), m_folderId},
        {QStringLiteral("name"), EncString::encryptString(QStringLiteral("Work"), m_userKey).toString()},
        {QStringLiteral("revisionDate"), QStringLiteral("2024-05-01T12:00:00.000Z")},
    };

    const auto makeCipher = [this](const QString &id,
                                   CipherType type,
                                   const QString &name,
                                   bool favorite,
                                   bool trashed,
                                   const QString &folderId,
                                   const QString &username = QString(),
                                   const QString &uri = QString()) {
        Cipher cipher;
        cipher.id = id;
        cipher.type = type;
        cipher.name = name;
        cipher.favorite = favorite;
        cipher.folderId = folderId;
        if (type == CipherType::Login) {
            cipher.username = username;
            cipher.password = QStringLiteral("hunter2");
            cipher.totp = QStringLiteral("JBSWY3DPEHPK3PXP");
            if (!uri.isEmpty()) {
                cipher.uris = {{uri, -1}};
            }
        }
        QJsonObject json = cipher.toEncryptedJson(m_userKey);
        json.insert(QStringLiteral("id"), id);
        json.insert(QStringLiteral("revisionDate"), QStringLiteral("2024-05-01T12:00:00.000Z"));
        if (trashed) {
            json.insert(QStringLiteral("deletedDate"), QStringLiteral("2024-06-01T12:00:00.000Z"));
        }
        return json;
    };

    QJsonArray ciphers{
        makeCipher(QStringLiteral("c1"),
                   CipherType::Login,
                   QStringLiteral("GitHub"),
                   true,
                   false,
                   m_folderId,
                   QStringLiteral("octocat"),
                   QStringLiteral("https://github.com/login")),
        makeCipher(QStringLiteral("c2"),
                   CipherType::Login,
                   QStringLiteral("GitLab"),
                   false,
                   false,
                   QString(),
                   QStringLiteral("tim"),
                   QStringLiteral("https://gitlab.com")),
        makeCipher(QStringLiteral("c3"), CipherType::SecureNote, QStringLiteral("Wifi password"), false, false, QString()),
        makeCipher(QStringLiteral("c4"), CipherType::Card, QStringLiteral("Travel card"), false, false, QString()),
        makeCipher(QStringLiteral("c5"), CipherType::Login, QStringLiteral("Old account"), false, true, QString()),
    };

    const QJsonObject payload{
        {QStringLiteral("profile"),
         QJsonObject{
             {QStringLiteral("id"), QStringLiteral("u1")},
             {QStringLiteral("email"), QString::fromLatin1(TestEmail)},
             {QStringLiteral("organizations"), QJsonArray{}},
         }},
        {QStringLiteral("folders"), QJsonArray{folder}},
        {QStringLiteral("ciphers"), ciphers},
    };

    QVERIFY(store.saveSyncPayload(payload));
}

void VaultTest::unlockAndWait(VaultManager &manager)
{
    manager.unlock(QString::fromLatin1(TestPassword));
    QTRY_COMPARE(manager.state(), VaultManager::Unlocked);
}

void VaultTest::testStartsLockedWithStoredAccount()
{
    VaultManager manager;
    QCOMPARE(manager.state(), VaultManager::Locked);
    QCOMPARE(manager.email(), QString::fromLatin1(TestEmail));
    // Nothing is readable before unlocking.
    QCOMPARE(manager.ciphers()->rowCount(), 0);
}

void VaultTest::testUnlockDecryptsCachedVault()
{
    VaultManager manager;
    QSignalSpy stateSpy(&manager, &VaultManager::stateChanged);

    manager.unlock(QString::fromLatin1(TestPassword));

    QTRY_COMPARE(manager.state(), VaultManager::Unlocked);
    QVERIFY(stateSpy.count() >= 1);

    QCOMPARE(manager.ciphers()->rowCount(), 5);
    QCOMPARE(manager.folders()->rowCount(), 1);
    QCOMPARE(manager.folders()->folderName(m_folderId), QStringLiteral("Work"));

    // Spot-check that fields really came back in the clear.
    const QVariantMap details = manager.cipherDetails(QStringLiteral("c1"));
    QCOMPARE(details.value(QStringLiteral("name")).toString(), QStringLiteral("GitHub"));
    QCOMPARE(details.value(QStringLiteral("username")).toString(), QStringLiteral("octocat"));
    QCOMPARE(details.value(QStringLiteral("password")).toString(), QStringLiteral("hunter2"));
    QCOMPARE(details.value(QStringLiteral("totp")).toString(), QStringLiteral("JBSWY3DPEHPK3PXP"));
    QVERIFY(details.value(QStringLiteral("favorite")).toBool());
    QCOMPARE(details.value(QStringLiteral("uris")).toList().size(), 1);

    // Counts used by the sidebar.
    QCOMPARE(manager.ciphers()->favoriteCount(), 1);
    QCOMPARE(manager.ciphers()->trashCount(), 1);
    QCOMPARE(manager.ciphers()->countInFolder(m_folderId), 1);
    QCOMPARE(manager.ciphers()->typeCount(CipherType::Login), 2); // the trashed one is excluded
}

void VaultTest::testWrongPasswordIsRejected()
{
    VaultManager manager;
    QSignalSpy failureSpy(&manager, &VaultManager::unlockFailed);

    manager.unlock(QStringLiteral("not the right password"));

    QTRY_COMPARE(failureSpy.count(), 1);
    QCOMPARE(manager.state(), VaultManager::Locked);
    QCOMPARE(manager.ciphers()->rowCount(), 0);

    // The correct password still works afterwards.
    unlockAndWait(manager);
}

void VaultTest::testLockClearsDecryptedItems()
{
    VaultManager manager;
    unlockAndWait(manager);
    QVERIFY(manager.ciphers()->rowCount() > 0);

    manager.lock();

    QCOMPARE(manager.state(), VaultManager::Locked);
    QCOMPARE(manager.ciphers()->rowCount(), 0);
    QCOMPARE(manager.folders()->rowCount(), 0);
    // Details must not be retrievable once locked.
    QVERIFY(manager.cipherDetails(QStringLiteral("c1")).isEmpty());

    // The account survives a lock, so unlocking again works.
    unlockAndWait(manager);
    QCOMPARE(manager.ciphers()->rowCount(), 5);
}

void VaultTest::testFilteringAndSearch()
{
    VaultManager manager;
    unlockAndWait(manager);

    auto *filter = manager.filteredCiphers();

    // Trashed items are hidden from every scope except the trash.
    filter->setScope(CipherFilterProxyModel::AllItems);
    QCOMPARE(filter->rowCount(), 4);

    filter->setScope(CipherFilterProxyModel::Trash);
    QCOMPARE(filter->rowCount(), 1);
    QCOMPARE(filter->index(0, 0).data(CipherListModel::NameRole).toString(), QStringLiteral("Old account"));

    filter->setScope(CipherFilterProxyModel::Favorites);
    QCOMPARE(filter->rowCount(), 1);

    filter->setScope(CipherFilterProxyModel::Folder);
    filter->setFolderId(m_folderId);
    QCOMPARE(filter->rowCount(), 1);

    filter->setScope(CipherFilterProxyModel::NoFolder);
    QCOMPARE(filter->rowCount(), 3);

    filter->setScope(CipherFilterProxyModel::Type);
    filter->setCipherType(int(CipherType::Card));
    QCOMPARE(filter->rowCount(), 1);

    // Search matches names...
    filter->setScope(CipherFilterProxyModel::AllItems);
    filter->setSearchText(QStringLiteral("git"));
    QCOMPARE(filter->rowCount(), 2);

    // ...and is case-insensitive, and matches usernames and hosts.
    filter->setSearchText(QStringLiteral("OCTOCAT"));
    QCOMPARE(filter->rowCount(), 1);
    filter->setSearchText(QStringLiteral("github.com"));
    QCOMPARE(filter->rowCount(), 1);
    // The bare host is indexed too, so a path in the URI does not hide it.
    filter->setSearchText(QStringLiteral("gitlab.com"));
    QCOMPARE(filter->rowCount(), 1);

    // Multiple terms must all match.
    filter->setSearchText(QStringLiteral("github octocat"));
    QCOMPARE(filter->rowCount(), 1);
    filter->setSearchText(QStringLiteral("github nonsense"));
    QCOMPARE(filter->rowCount(), 0);

    // Secret values are never searchable.
    filter->setSearchText(QStringLiteral("hunter2"));
    QCOMPARE(filter->rowCount(), 0);

    filter->setSearchText(QString());
    QCOMPARE(filter->rowCount(), 4);

    // Favourites sort ahead of everything else.
    QCOMPARE(filter->index(0, 0).data(CipherListModel::NameRole).toString(), QStringLiteral("GitHub"));
}

void VaultTest::testEditorRoundTrip()
{
    VaultManager manager;
    unlockAndWait(manager);

    QVERIFY(manager.beginEdit(QStringLiteral("c1")));
    auto *editor = manager.editor();
    QCOMPARE(editor->isNew(), false);
    QCOMPARE(editor->property("name").toString(), QStringLiteral("GitHub"));
    QCOMPARE(editor->property("username").toString(), QStringLiteral("octocat"));
    QCOMPARE(editor->property("uris").toList().size(), 1);

    // Changing the password should file the old one into history.
    editor->setProperty("password", QStringLiteral("new-password"));
    const Cipher edited = editor->toCipher();
    QCOMPARE(edited.password, QStringLiteral("new-password"));
    QCOMPARE(edited.passwordHistory.size(), 1);
    QCOMPARE(edited.passwordHistory.first().password, QStringLiteral("hunter2"));
    QVERIFY(edited.passwordRevisionDate.isValid());
    // Unrelated fields must survive untouched.
    QCOMPARE(edited.id, QStringLiteral("c1"));
    QCOMPARE(edited.username, QStringLiteral("octocat"));
    QCOMPARE(edited.totp, QStringLiteral("JBSWY3DPEHPK3PXP"));

    // A new item starts empty and is marked as such.
    manager.beginCreate(int(CipherType::Card), m_folderId);
    QCOMPARE(editor->isNew(), true);
    QCOMPARE(editor->property("type").toInt(), int(CipherType::Card));
    QCOMPARE(editor->property("folderId").toString(), m_folderId);
    QVERIFY(editor->property("name").toString().isEmpty());
    QVERIFY(editor->property("password").toString().isEmpty());

    // Editing a non-existent item must fail rather than show stale data.
    QCOMPARE(manager.beginEdit(QStringLiteral("does-not-exist")), false);
}

void VaultTest::testLogoutRemovesCachedVault()
{
    VaultManager manager;
    unlockAndWait(manager);

    manager.logout();

    QCOMPARE(manager.state(), VaultManager::LoggedOut);
    QCOMPARE(manager.ciphers()->rowCount(), 0);
    QVERIFY(manager.email().isEmpty());

    // The offline copy must really be gone from disk.
    LocalStore store;
    QVERIFY(!store.hasCachedVault());
    QVERIFY(!store.loadAccount().has_value());

    // A fresh manager therefore starts logged out.
    VaultManager restarted;
    QCOMPARE(restarted.state(), VaultManager::LoggedOut);
}

QTEST_GUILESS_MAIN(VaultTest)
#include "vaulttest.moc"
