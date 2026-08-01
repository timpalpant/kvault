#include "passwordgenerator.h"

#include "crypto/wordlist.h"

#include <QRandomGenerator>
#include <QtMath>

namespace kvault {

namespace {

// Characters that are easy to confuse in a monospace font are dropped when
// "avoid ambiguous" is on.
constexpr const char *Uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr const char *UppercaseUnambiguous = "ABCDEFGHJKLMNPQRSTUVWXYZ";
constexpr const char *Lowercase = "abcdefghijklmnopqrstuvwxyz";
constexpr const char *LowercaseUnambiguous = "abcdefghijkmnopqrstuvwxyz";
constexpr const char *Digits = "0123456789";
constexpr const char *DigitsUnambiguous = "23456789";
constexpr const char *Special = "!@#$%^&*";

/// QRandomGenerator::system() draws from the OS CSPRNG, and bounded() is unbiased.
int randomBelow(int bound)
{
    return bound > 0 ? int(QRandomGenerator::system()->bounded(bound)) : 0;
}

QChar randomFrom(const QString &pool)
{
    return pool.isEmpty() ? QChar() : pool.at(randomBelow(int(pool.size())));
}

} // namespace

PasswordGenerator::PasswordGenerator(QObject *parent)
    : QObject(parent)
{
    regenerate();
}

void PasswordGenerator::regenerate()
{
    m_value = (m_mode == Passphrase) ? generatePassphrase() : generatePassword();
    Q_EMIT valueChanged();
}

QString PasswordGenerator::generatePassword() const
{
    const QString upper = QLatin1String(m_avoidAmbiguous ? UppercaseUnambiguous : Uppercase);
    const QString lower = QLatin1String(m_avoidAmbiguous ? LowercaseUnambiguous : Lowercase);
    const QString digits = QLatin1String(m_avoidAmbiguous ? DigitsUnambiguous : Digits);
    const QString special = QLatin1String(Special);

    QString pool;
    if (m_useUppercase) {
        pool += upper;
    }
    if (m_useLowercase) {
        pool += lower;
    }
    if (m_useDigits) {
        pool += digits;
    }
    if (m_useSpecial) {
        pool += special;
    }
    if (pool.isEmpty()) {
        // Every character class was switched off; fall back to lowercase rather
        // than returning an empty password.
        pool = lower;
    }

    const int length = qBound(5, m_length, 128);
    QList<QChar> characters;
    characters.reserve(length);

    // Satisfy the "at least one of" and minimum-count rules first.
    const auto require = [&](const QString &set, int count) {
        for (int i = 0; i < count && characters.size() < length; ++i) {
            characters.append(randomFrom(set));
        }
    };
    if (m_useUppercase) {
        require(upper, 1);
    }
    if (m_useLowercase) {
        require(lower, 1);
    }
    if (m_useDigits) {
        require(digits, qMax(1, m_minDigits));
    }
    if (m_useSpecial) {
        require(special, qMax(1, m_minSpecial));
    }

    while (characters.size() < length) {
        characters.append(randomFrom(pool));
    }

    // Fisher-Yates, so the required characters are not stuck at the front.
    for (int i = int(characters.size()) - 1; i > 0; --i) {
        const int j = randomBelow(i + 1);
        characters.swapItemsAt(i, j);
    }

    QString result;
    result.reserve(length);
    for (const QChar c : characters) {
        result.append(c);
    }
    return result;
}

QString PasswordGenerator::generatePassphrase() const
{
    const int count = qBound(3, m_wordCount, 20);
    const int listSize = Wordlist::size();

    QStringList words;
    words.reserve(count);
    for (int i = 0; i < count; ++i) {
        QString word = Wordlist::word(randomBelow(listSize));
        if (m_capitalise && !word.isEmpty()) {
            word[0] = word.at(0).toUpper();
        }
        words.append(word);
    }

    if (m_includeNumber && !words.isEmpty()) {
        const int index = randomBelow(int(words.size()));
        words[index].append(QString::number(randomBelow(10)));
    }

    return words.join(m_separator);
}

int PasswordGenerator::estimateStrength(const QString &password)
{
    if (password.isEmpty()) {
        return 0;
    }

    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;
    for (const QChar c : password) {
        if (c.isLower()) {
            hasLower = true;
        } else if (c.isUpper()) {
            hasUpper = true;
        } else if (c.isDigit()) {
            hasDigit = true;
        } else {
            hasSpecial = true;
        }
    }

    int poolSize = 0;
    poolSize += hasLower ? 26 : 0;
    poolSize += hasUpper ? 26 : 0;
    poolSize += hasDigit ? 10 : 0;
    poolSize += hasSpecial ? 33 : 0;
    if (poolSize == 0) {
        return 0;
    }

    // Entropy of a random string over the observed alphabet. This overestimates
    // human-chosen passwords, so treat it as a hint rather than a verdict.
    const double entropy = password.size() * std::log2(double(poolSize));

    if (entropy < 28) {
        return 0;
    }
    if (entropy < 40) {
        return 1;
    }
    if (entropy < 60) {
        return 2;
    }
    if (entropy < 100) {
        return 3;
    }
    return 4;
}

int PasswordGenerator::strength() const
{
    return estimateStrength(m_value);
}

void PasswordGenerator::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    Q_EMIT optionsChanged();
    regenerate();
}

#define KVAULT_SETTER(setter, member, type)    \
    void PasswordGenerator::setter(type value) \
    {                                          \
        if (member == value) {                 \
            return;                            \
        }                                      \
        member = value;                        \
        Q_EMIT optionsChanged();               \
        regenerate();                          \
    }

KVAULT_SETTER(setLength, m_length, int)
KVAULT_SETTER(setUseUppercase, m_useUppercase, bool)
KVAULT_SETTER(setUseLowercase, m_useLowercase, bool)
KVAULT_SETTER(setUseDigits, m_useDigits, bool)
KVAULT_SETTER(setUseSpecial, m_useSpecial, bool)
KVAULT_SETTER(setAvoidAmbiguous, m_avoidAmbiguous, bool)
KVAULT_SETTER(setMinDigits, m_minDigits, int)
KVAULT_SETTER(setMinSpecial, m_minSpecial, int)
KVAULT_SETTER(setWordCount, m_wordCount, int)
KVAULT_SETTER(setCapitalise, m_capitalise, bool)
KVAULT_SETTER(setIncludeNumber, m_includeNumber, bool)
KVAULT_SETTER(setSeparator, m_separator, const QString &)

#undef KVAULT_SETTER

} // namespace kvault
