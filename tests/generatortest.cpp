#include "app/passwordgenerator.h"
#include "crypto/wordlist.h"

#include <QSet>
#include <QTest>

using namespace kvault;

namespace {

bool containsAny(const QString &text, const QString &set)
{
    for (const QChar c : text) {
        if (set.contains(c)) {
            return true;
        }
    }
    return false;
}

int countIn(const QString &text, const QString &set)
{
    int count = 0;
    for (const QChar c : text) {
        if (set.contains(c)) {
            ++count;
        }
    }
    return count;
}

constexpr const char *Upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr const char *Lower = "abcdefghijklmnopqrstuvwxyz";
constexpr const char *DigitChars = "0123456789";
constexpr const char *SpecialChars = "!@#$%^&*";
/// The characters "avoid ambiguous" is supposed to remove.
constexpr const char *Ambiguous = "IlO01";

} // namespace

class GeneratorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testWordlistIntegrity();
    void testDefaultPassword();
    void testLengthIsHonoured();
    void testCharacterClasses();
    void testMinimumCounts();
    void testAvoidAmbiguous();
    void testNoClassesEnabled();
    void testPasswordsAreNotRepeated();
    void testPassphrase();
    void testPassphraseOptions();
    void testStrengthEstimate();
};

void GeneratorTest::testWordlistIntegrity()
{
    QCOMPARE(Wordlist::size(), 7776);
    QVERIFY(!Wordlist::word(0).isEmpty());
    QVERIFY(!Wordlist::word(7775).isEmpty());
    // Out-of-range access must be safe rather than undefined.
    QVERIFY(Wordlist::word(-1).isEmpty());
    QVERIFY(Wordlist::word(7776).isEmpty());

    // Spot-check that the list really is the EFF one and is unique.
    QCOMPARE(Wordlist::word(0), QStringLiteral("abacus"));
    QSet<QString> seen;
    for (int i = 0; i < Wordlist::size(); ++i) {
        seen.insert(Wordlist::word(i));
    }
    QCOMPARE(seen.size(), 7776);
}

void GeneratorTest::testDefaultPassword()
{
    PasswordGenerator generator;
    QCOMPARE(generator.mode(), PasswordGenerator::Password);
    QCOMPARE(generator.value().size(), 16);
}

void GeneratorTest::testLengthIsHonoured()
{
    PasswordGenerator generator;
    for (int length : {5, 8, 16, 32, 64, 128}) {
        generator.setLength(length);
        QCOMPARE(generator.value().size(), length);
    }

    // Out-of-range lengths are clamped, not passed through.
    generator.setLength(1);
    QCOMPARE(generator.value().size(), 5);
    generator.setLength(1000);
    QCOMPARE(generator.value().size(), 128);
}

void GeneratorTest::testCharacterClasses()
{
    PasswordGenerator generator;
    generator.setLength(64);

    generator.setUseUppercase(true);
    generator.setUseLowercase(false);
    generator.setUseDigits(false);
    generator.setUseSpecial(false);
    for (int i = 0; i < 20; ++i) {
        generator.regenerate();
        const QString value = generator.value();
        QVERIFY2(!containsAny(value, QLatin1String(Lower)), qPrintable(value));
        QVERIFY2(!containsAny(value, QLatin1String(DigitChars)), qPrintable(value));
        QVERIFY2(!containsAny(value, QLatin1String(SpecialChars)), qPrintable(value));
    }

    generator.setUseUppercase(false);
    generator.setUseDigits(true);
    for (int i = 0; i < 20; ++i) {
        generator.regenerate();
        const QString value = generator.value();
        QVERIFY2(!containsAny(value, QLatin1String(Upper)), qPrintable(value));
        QVERIFY2(!containsAny(value, QLatin1String(SpecialChars)), qPrintable(value));
    }

    // With everything enabled, each class should show up at least once.
    generator.setUseUppercase(true);
    generator.setUseLowercase(true);
    generator.setUseSpecial(true);
    for (int i = 0; i < 20; ++i) {
        generator.regenerate();
        const QString value = generator.value();
        QVERIFY2(containsAny(value, QLatin1String(Upper)), qPrintable(value));
        QVERIFY2(containsAny(value, QLatin1String(Lower)), qPrintable(value));
        QVERIFY2(containsAny(value, QLatin1String(DigitChars)), qPrintable(value));
        QVERIFY2(containsAny(value, QLatin1String(SpecialChars)), qPrintable(value));
    }
}

void GeneratorTest::testMinimumCounts()
{
    PasswordGenerator generator;
    generator.setLength(32);
    generator.setUseUppercase(true);
    generator.setUseLowercase(true);
    generator.setUseDigits(true);
    generator.setUseSpecial(true);
    generator.setMinDigits(5);
    generator.setMinSpecial(4);

    for (int i = 0; i < 30; ++i) {
        generator.regenerate();
        const QString value = generator.value();
        QVERIFY2(countIn(value, QLatin1String(DigitChars)) >= 5, qPrintable(value));
        QVERIFY2(countIn(value, QLatin1String(SpecialChars)) >= 4, qPrintable(value));
    }
}

void GeneratorTest::testAvoidAmbiguous()
{
    PasswordGenerator generator;
    generator.setLength(128);
    generator.setUseUppercase(true);
    generator.setUseLowercase(true);
    generator.setUseDigits(true);
    generator.setUseSpecial(false);
    generator.setAvoidAmbiguous(true);

    for (int i = 0; i < 30; ++i) {
        generator.regenerate();
        const QString value = generator.value();
        QVERIFY2(!containsAny(value, QLatin1String(Ambiguous)), qPrintable(value));
    }
}

void GeneratorTest::testNoClassesEnabled()
{
    // Turning everything off must still produce a usable password rather than
    // an empty string.
    PasswordGenerator generator;
    generator.setLength(20);
    generator.setUseUppercase(false);
    generator.setUseLowercase(false);
    generator.setUseDigits(false);
    generator.setUseSpecial(false);

    const QString value = generator.value();
    QCOMPARE(value.size(), 20);
    QVERIFY(containsAny(value, QLatin1String(Lower)));
}

void GeneratorTest::testPasswordsAreNotRepeated()
{
    PasswordGenerator generator;
    generator.setLength(24);

    QSet<QString> values;
    for (int i = 0; i < 200; ++i) {
        generator.regenerate();
        values.insert(generator.value());
    }
    // 24 characters from a large alphabet: collisions would mean a broken RNG.
    QCOMPARE(values.size(), 200);
}

void GeneratorTest::testPassphrase()
{
    PasswordGenerator generator;
    generator.setMode(PasswordGenerator::Passphrase);
    generator.setWordCount(4);
    // The EFF list legitimately contains hyphenated words, so use a delimiter
    // that cannot be confused with a word boundary when inspecting the output.
    generator.setSeparator(QStringLiteral("|"));
    generator.setCapitalise(false);
    generator.setIncludeNumber(false);

    for (int i = 0; i < 20; ++i) {
        generator.regenerate();
        const QStringList words = generator.value().split(QLatin1Char('|'));
        QCOMPARE(words.size(), 4);
        for (const QString &word : words) {
            QVERIFY2(!word.isEmpty(), qPrintable(generator.value()));
            QCOMPARE(word, word.toLower());
        }
    }
}

void GeneratorTest::testPassphraseOptions()
{
    PasswordGenerator generator;
    generator.setMode(PasswordGenerator::Passphrase);
    generator.setWordCount(5);
    generator.setSeparator(QStringLiteral("."));
    generator.setCapitalise(true);
    generator.setIncludeNumber(true);

    for (int i = 0; i < 20; ++i) {
        generator.regenerate();
        const QString value = generator.value();
        const QStringList words = value.split(QLatin1Char('.'));
        QCOMPARE(words.size(), 5);

        for (const QString &word : words) {
            QVERIFY2(word.at(0).isUpper(), qPrintable(value));
        }
        // Exactly one word carries the appended digit.
        QCOMPARE(countIn(value, QLatin1String(DigitChars)), 1);
    }

    // Word count is clamped to a sane range.
    generator.setWordCount(1);
    QCOMPARE(generator.value().split(QLatin1Char('.')).size(), 3);
    generator.setWordCount(500);
    QCOMPARE(generator.value().split(QLatin1Char('.')).size(), 20);
}

void GeneratorTest::testStrengthEstimate()
{
    QCOMPARE(PasswordGenerator::estimateStrength(QString()), 0);
    QCOMPARE(PasswordGenerator::estimateStrength(QStringLiteral("abc")), 0);

    // Longer and more varied must never score lower.
    const int weak = PasswordGenerator::estimateStrength(QStringLiteral("password"));
    const int medium = PasswordGenerator::estimateStrength(QStringLiteral("Password12"));
    const int strong = PasswordGenerator::estimateStrength(QStringLiteral("Xk9#mQ2$vL7@nR4!"));
    const int veryStrong = PasswordGenerator::estimateStrength(QStringLiteral("Xk9#mQ2$vL7@nR4!pT6&wY1*zB8^cF3%"));

    QVERIFY(weak <= medium);
    QVERIFY(medium < strong);
    QVERIFY(strong <= veryStrong);
    QCOMPARE(veryStrong, 4);

    // A generated default should not read as weak.
    PasswordGenerator generator;
    QVERIFY(generator.strength() >= 3);
}

QTEST_GUILESS_MAIN(GeneratorTest)
#include "generatortest.moc"
