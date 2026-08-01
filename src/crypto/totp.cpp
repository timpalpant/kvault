#include "totp.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <QUrl>
#include <QUrlQuery>

namespace kvault {

namespace {

constexpr char SteamAlphabet[] = "23456789BCDFGHJKMNPQRTVWXY";
constexpr int SteamAlphabetSize = 26;

QByteArray hmac(Totp::Algorithm algorithm, const QByteArray &key, const QByteArray &data)
{
    const EVP_MD *md = nullptr;
    switch (algorithm) {
    case Totp::Algorithm::Sha1:
        md = EVP_sha1();
        break;
    case Totp::Algorithm::Sha256:
        md = EVP_sha256();
        break;
    case Totp::Algorithm::Sha512:
        md = EVP_sha512();
        break;
    }

    QByteArray out(EVP_MAX_MD_SIZE, Qt::Uninitialized);
    unsigned int len = 0;
    const unsigned char *result = HMAC(md,
                                       key.constData(),
                                       int(key.size()),
                                       reinterpret_cast<const unsigned char *>(data.constData()),
                                       size_t(data.size()),
                                       reinterpret_cast<unsigned char *>(out.data()),
                                       &len);
    if (!result) {
        return {};
    }
    out.resize(qsizetype(len));
    return out;
}

Totp::Algorithm algorithmFromString(const QString &name)
{
    const QString upper = name.trimmed().toUpper();
    if (upper == QLatin1String("SHA256")) {
        return Totp::Algorithm::Sha256;
    }
    if (upper == QLatin1String("SHA512")) {
        return Totp::Algorithm::Sha512;
    }
    return Totp::Algorithm::Sha1;
}

} // namespace

QByteArray base32Decode(const QString &input)
{
    QByteArray out;
    quint32 buffer = 0;
    int bitsLeft = 0;

    for (const QChar qc : input) {
        const char c = qc.toUpper().toLatin1();
        int value = -1;
        if (c >= 'A' && c <= 'Z') {
            value = c - 'A';
        } else if (c >= '2' && c <= '7') {
            value = c - '2' + 26;
        } else if (c == '=' || c == ' ' || c == '-' || c == '\t') {
            continue;
        } else {
            return {}; // not base32
        }

        buffer = (buffer << 5) | quint32(value);
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            bitsLeft -= 8;
            out.append(char((buffer >> bitsLeft) & 0xff));
        }
    }
    return out;
}

Totp Totp::parse(const QString &keyOrUri)
{
    Totp totp;
    const QString trimmed = keyOrUri.trimmed();
    if (trimmed.isEmpty()) {
        return totp;
    }

    if (trimmed.startsWith(QLatin1String("steam://"), Qt::CaseInsensitive)) {
        totp.m_secret = base32Decode(trimmed.mid(8));
        totp.m_steam = true;
        totp.m_digits = 5;
        totp.m_period = 30;
        totp.m_valid = !totp.m_secret.isEmpty();
        return totp;
    }

    if (trimmed.startsWith(QLatin1String("otpauth://"), Qt::CaseInsensitive)) {
        const QUrl url(trimmed);
        const QUrlQuery query(url);

        totp.m_secret = base32Decode(query.queryItemValue(QStringLiteral("secret"), QUrl::FullyDecoded));
        totp.m_algorithm = algorithmFromString(query.queryItemValue(QStringLiteral("algorithm")));

        bool ok = false;
        const int digits = query.queryItemValue(QStringLiteral("digits")).toInt(&ok);
        if (ok && digits >= 4 && digits <= 10) {
            totp.m_digits = digits;
        }
        const int period = query.queryItemValue(QStringLiteral("period")).toInt(&ok);
        if (ok && period > 0) {
            totp.m_period = period;
        }

        const QString encoder = query.queryItemValue(QStringLiteral("encoder"));
        if (encoder.compare(QLatin1String("steam"), Qt::CaseInsensitive) == 0
            || url.host().compare(QLatin1String("steam"), Qt::CaseInsensitive) == 0) {
            totp.m_steam = true;
            totp.m_digits = 5;
        }

        totp.m_issuer = query.queryItemValue(QStringLiteral("issuer"), QUrl::FullyDecoded);

        // The label is "Issuer:account" or just "account".
        QString label = url.path(QUrl::FullyDecoded);
        if (label.startsWith(u'/')) {
            label.remove(0, 1);
        }
        const qsizetype colon = label.indexOf(u':');
        if (colon >= 0) {
            if (totp.m_issuer.isEmpty()) {
                totp.m_issuer = label.left(colon);
            }
            totp.m_account = label.mid(colon + 1).trimmed();
        } else {
            totp.m_account = label;
        }

        totp.m_valid = !totp.m_secret.isEmpty();
        return totp;
    }

    // Bare base32 secret.
    totp.m_secret = base32Decode(trimmed);
    totp.m_valid = !totp.m_secret.isEmpty();
    return totp;
}

QString Totp::code(qint64 unixSeconds) const
{
    if (!m_valid || m_period <= 0) {
        return {};
    }

    const quint64 counter = quint64(unixSeconds / m_period);
    QByteArray message(8, Qt::Uninitialized);
    for (int i = 7; i >= 0; --i) {
        message[i] = char((counter >> ((7 - i) * 8)) & 0xff);
    }

    const QByteArray digest = hmac(m_algorithm, m_secret, message);
    if (digest.size() < 20) {
        return {};
    }

    const int offset = digest.at(digest.size() - 1) & 0x0f;
    const quint32 truncated = ((quint32(digest.at(offset)) & 0x7f) << 24) | ((quint32(digest.at(offset + 1)) & 0xff) << 16)
                              | ((quint32(digest.at(offset + 2)) & 0xff) << 8) | (quint32(digest.at(offset + 3)) & 0xff);

    if (m_steam) {
        QString out;
        quint32 value = truncated;
        for (int i = 0; i < 5; ++i) {
            out.append(QLatin1Char(SteamAlphabet[value % SteamAlphabetSize]));
            value /= SteamAlphabetSize;
        }
        return out;
    }

    quint32 modulus = 1;
    for (int i = 0; i < m_digits; ++i) {
        modulus *= 10;
    }
    return QStringLiteral("%1").arg(truncated % modulus, m_digits, 10, QLatin1Char('0'));
}

int Totp::secondsRemaining(qint64 unixSeconds) const
{
    if (!m_valid || m_period <= 0) {
        return 0;
    }
    return m_period - int(unixSeconds % m_period);
}

} // namespace kvault
