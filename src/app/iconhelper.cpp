#include "iconhelper.h"

#include "model/cipher.h"
#include "passwordgenerator.h"

#include <QColor>
#include <QCryptographicHash>
#include <QLocale>
#include <QUrl>

#include <KLocalizedString>

namespace kvault {

IconHelper::IconHelper(QObject *parent)
    : QObject(parent)
{}

QString IconHelper::iconForType(int cipherType) const
{
    switch (CipherType(cipherType)) {
    case CipherType::Login:
        return QStringLiteral("password-show-on");
    case CipherType::SecureNote:
        return QStringLiteral("note");
    case CipherType::Card:
        return QStringLiteral("credit-card");
    case CipherType::Identity:
        return QStringLiteral("user-identity");
    case CipherType::SshKey:
        return QStringLiteral("network-server");
    }
    return QStringLiteral("unknown");
}

QString IconHelper::labelForType(int cipherType) const
{
    switch (CipherType(cipherType)) {
    case CipherType::Login:
        return i18n("Login");
    case CipherType::SecureNote:
        return i18n("Secure note");
    case CipherType::Card:
        return i18n("Card");
    case CipherType::Identity:
        return i18n("Identity");
    case CipherType::SshKey:
        return i18n("SSH key");
    }
    return i18n("Item");
}

QString IconHelper::avatarLetter(const QString &name) const
{
    for (const QChar c : name) {
        if (c.isLetterOrNumber()) {
            return QString(c.toUpper());
        }
    }
    return QStringLiteral("?");
}

QColor IconHelper::avatarColor(const QString &name) const
{
    // Hash so the colour is stable across restarts and independent of ordering.
    const QByteArray digest = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256);
    const int hue = int(uint8_t(digest.at(0))) * 360 / 256;
    // Fixed saturation and lightness keep every avatar legible against both
    // light and dark backgrounds.
    return QColor::fromHsl(hue, 140, 120);
}

QString IconHelper::prettyHost(const QString &uri) const
{
    if (uri.isEmpty()) {
        return {};
    }
    QUrl url(uri);
    if (url.scheme().isEmpty()) {
        url = QUrl(QStringLiteral("https://") + uri);
    }
    QString host = url.host();
    if (host.isEmpty()) {
        return uri;
    }
    if (host.startsWith(QLatin1String("www."))) {
        host.remove(0, 4);
    }
    return host;
}

QString IconHelper::formatFileSize(qint64 bytes) const
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

int IconHelper::passwordStrength(const QString &password) const
{
    return PasswordGenerator::estimateStrength(password);
}

} // namespace kvault
