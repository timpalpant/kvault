#include "crypto/totp.h"

#include <QTest>

using namespace kvault;

// RFC 6238 uses ASCII digits repeated to the digest length as the shared secret.
namespace {
constexpr const char *Sha1Secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
constexpr const char *Sha256Secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZA";
constexpr const char *Sha512Secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"
                                     "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNA";
} // namespace

class TotpTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBase32Decode();
    void testRfc6238Sha1();
    void testRfc6238OtherAlgorithms();
    void testBareSecret();
    void testOtpauthUri();
    void testSteam();
    void testCountdown();
    void testInvalidInput();
};

void TotpTest::testBase32Decode()
{
    QCOMPARE(base32Decode(QStringLiteral("MZXW6===")), QByteArrayLiteral("foo"));
    QCOMPARE(base32Decode(QStringLiteral("MZXW6YTBOI======")), QByteArrayLiteral("fooba r").replace(' ', ""));
    QCOMPARE(base32Decode(QStringLiteral("JBSWY3DP")), QByteArrayLiteral("Hello"));
    // Case, spaces and dashes are all tolerated, as users paste them.
    QCOMPARE(base32Decode(QStringLiteral("jbsw y3dp")), QByteArrayLiteral("Hello"));
    QCOMPARE(base32Decode(QStringLiteral("JBSW-Y3DP")), QByteArrayLiteral("Hello"));
    // Characters outside the alphabet mean this is not a base32 secret.
    QCOMPARE(base32Decode(QStringLiteral("not base32!")), QByteArray());
}

void TotpTest::testRfc6238Sha1()
{
    const Totp totp = Totp::parse(QStringLiteral("otpauth://totp/test?secret=%1&digits=8").arg(QLatin1String(Sha1Secret)));
    QVERIFY(totp.isValid());
    QCOMPARE(totp.digits(), 8);
    QCOMPARE(totp.period(), 30);

    QCOMPARE(totp.code(59), QStringLiteral("94287082"));
    QCOMPARE(totp.code(1111111109), QStringLiteral("07081804"));
    QCOMPARE(totp.code(1111111111), QStringLiteral("14050471"));
    QCOMPARE(totp.code(1234567890), QStringLiteral("89005924"));
    QCOMPARE(totp.code(2000000000), QStringLiteral("69279037"));
}

void TotpTest::testRfc6238OtherAlgorithms()
{
    const Totp sha256 = Totp::parse(QStringLiteral("otpauth://totp/test?secret=%1&digits=8&algorithm=SHA256").arg(QLatin1String(Sha256Secret)));
    QVERIFY(sha256.isValid());
    QCOMPARE(sha256.code(59), QStringLiteral("46119246"));

    const Totp sha512 = Totp::parse(QStringLiteral("otpauth://totp/test?secret=%1&digits=8&algorithm=SHA512").arg(QLatin1String(Sha512Secret)));
    QVERIFY(sha512.isValid());
    QCOMPARE(sha512.code(59), QStringLiteral("90693936"));
}

void TotpTest::testBareSecret()
{
    // The common case: Bitwarden stores just the base32 secret.
    const Totp totp = Totp::parse(QString::fromLatin1(Sha1Secret));
    QVERIFY(totp.isValid());
    QCOMPARE(totp.digits(), 6);
    QCOMPARE(totp.period(), 30);
    // Same as the RFC vector, truncated to six digits.
    QCOMPARE(totp.code(59), QStringLiteral("287082"));

    // Surrounding whitespace is common when pasting.
    QVERIFY(Totp::parse(QStringLiteral("  JBSWY3DP  ")).isValid());
}

void TotpTest::testOtpauthUri()
{
    const Totp totp
        = Totp::parse(QStringLiteral("otpauth://totp/GitHub:octocat?secret=JBSWY3DPEHPK3PXP&issuer=GitHub&period=60&digits=7&algorithm=SHA256"));
    QVERIFY(totp.isValid());
    QCOMPARE(totp.period(), 60);
    QCOMPARE(totp.digits(), 7);
    QCOMPARE(totp.issuer(), QStringLiteral("GitHub"));
    QCOMPARE(totp.accountName(), QStringLiteral("octocat"));
    QCOMPARE(totp.code(0).size(), 7);

    // A label without an issuer prefix.
    const Totp bare = Totp::parse(QStringLiteral("otpauth://totp/octocat?secret=JBSWY3DPEHPK3PXP"));
    QCOMPARE(bare.accountName(), QStringLiteral("octocat"));
    QVERIFY(bare.issuer().isEmpty());
}

void TotpTest::testSteam()
{
    for (const QString &input :
         {QStringLiteral("steam://JBSWY3DPEHPK3PXP"), QStringLiteral("otpauth://totp/steam?secret=JBSWY3DPEHPK3PXP&encoder=steam")}) {
        const Totp totp = Totp::parse(input);
        QVERIFY2(totp.isValid(), qPrintable(input));
        QVERIFY(totp.isSteam());
        QCOMPARE(totp.digits(), 5);

        const QString code = totp.code(1000000000);
        QCOMPARE(code.size(), 5);
        // Steam codes use a restricted alphabet with no confusable characters.
        for (const QChar c : code) {
            QVERIFY2(QStringLiteral("23456789BCDFGHJKMNPQRTVWXY").contains(c), qPrintable(code));
        }
    }
}

void TotpTest::testCountdown()
{
    const Totp totp = Totp::parse(QStringLiteral("JBSWY3DPEHPK3PXP"));
    QCOMPARE(totp.secondsRemaining(0), 30);
    QCOMPARE(totp.secondsRemaining(1), 29);
    QCOMPARE(totp.secondsRemaining(29), 1);
    QCOMPARE(totp.secondsRemaining(30), 30);

    // The code only changes when the interval does.
    QCOMPARE(totp.code(0), totp.code(29));
    QVERIFY(totp.code(0) != totp.code(30));
}

void TotpTest::testInvalidInput()
{
    QVERIFY(!Totp::parse(QString()).isValid());
    QVERIFY(!Totp::parse(QStringLiteral("   ")).isValid());
    QVERIFY(!Totp::parse(QStringLiteral("this is not base32!")).isValid());
    QVERIFY(!Totp::parse(QStringLiteral("otpauth://totp/test?issuer=NoSecret")).isValid());
    QVERIFY(Totp::parse(QString()).code(0).isEmpty());
}

QTEST_GUILESS_MAIN(TotpTest)
#include "totptest.moc"
