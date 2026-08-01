#include "appsettings.h"

#include <QUuid>

namespace kvault {

namespace {
constexpr const char *KeyServerUrl = "server/url";
constexpr const char *KeyLastEmail = "account/lastEmail";
constexpr const char *KeyDeviceId = "account/deviceIdentifier";
constexpr const char *KeyLockTimeout = "security/lockTimeoutMinutes";
constexpr const char *KeyLockOnMinimize = "security/lockOnMinimize";
constexpr const char *KeyClipboardClear = "security/clipboardClearSeconds";
constexpr const char *KeySyncOnUnlock = "sync/onUnlock";
constexpr const char *KeyConcealPasswords = "ui/concealPasswords";
constexpr const char *KeyCloseToTray = "ui/closeToTray";
constexpr const char *KeyColumnState = "ui/columnState";
} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{}

QString AppSettings::serverUrl() const
{
    return m_settings.value(QLatin1String(KeyServerUrl), QStringLiteral("https://vault.bitwarden.com")).toString();
}

void AppSettings::setServerUrl(const QString &url)
{
    if (serverUrl() == url) {
        return;
    }
    m_settings.setValue(QLatin1String(KeyServerUrl), url);
    Q_EMIT serverUrlChanged();
}

QString AppSettings::lastEmail() const
{
    return m_settings.value(QLatin1String(KeyLastEmail)).toString();
}

void AppSettings::setLastEmail(const QString &email)
{
    if (lastEmail() == email) {
        return;
    }
    m_settings.setValue(QLatin1String(KeyLastEmail), email);
    Q_EMIT lastEmailChanged();
}

int AppSettings::lockTimeoutMinutes() const
{
    return m_settings.value(QLatin1String(KeyLockTimeout), 15).toInt();
}

void AppSettings::setLockTimeoutMinutes(int minutes)
{
    if (lockTimeoutMinutes() == minutes) {
        return;
    }
    m_settings.setValue(QLatin1String(KeyLockTimeout), minutes);
    Q_EMIT lockTimeoutMinutesChanged();
}

bool AppSettings::lockOnMinimize() const
{
    return m_settings.value(QLatin1String(KeyLockOnMinimize), false).toBool();
}

void AppSettings::setLockOnMinimize(bool lock)
{
    if (lockOnMinimize() == lock) {
        return;
    }
    m_settings.setValue(QLatin1String(KeyLockOnMinimize), lock);
    Q_EMIT lockOnMinimizeChanged();
}

int AppSettings::clipboardClearSeconds() const
{
    return m_settings.value(QLatin1String(KeyClipboardClear), 30).toInt();
}

void AppSettings::setClipboardClearSeconds(int seconds)
{
    if (clipboardClearSeconds() == seconds) {
        return;
    }
    m_settings.setValue(QLatin1String(KeyClipboardClear), seconds);
    Q_EMIT clipboardClearSecondsChanged();
}

bool AppSettings::syncOnUnlock() const
{
    return m_settings.value(QLatin1String(KeySyncOnUnlock), true).toBool();
}

void AppSettings::setSyncOnUnlock(bool sync)
{
    if (syncOnUnlock() == sync) {
        return;
    }
    m_settings.setValue(QLatin1String(KeySyncOnUnlock), sync);
    Q_EMIT syncOnUnlockChanged();
}

bool AppSettings::concealPasswords() const
{
    return m_settings.value(QLatin1String(KeyConcealPasswords), true).toBool();
}

void AppSettings::setConcealPasswords(bool conceal)
{
    if (concealPasswords() == conceal) {
        return;
    }
    m_settings.setValue(QLatin1String(KeyConcealPasswords), conceal);
    Q_EMIT concealPasswordsChanged();
}

bool AppSettings::closeToTray() const
{
    return m_settings.value(QLatin1String(KeyCloseToTray), false).toBool();
}

void AppSettings::setCloseToTray(bool closeToTray)
{
    if (this->closeToTray() == closeToTray) {
        return;
    }
    m_settings.setValue(QLatin1String(KeyCloseToTray), closeToTray);
    Q_EMIT closeToTrayChanged();
}

QString AppSettings::columnState() const
{
    return m_settings.value(QLatin1String(KeyColumnState)).toString();
}

void AppSettings::setColumnState(const QString &state)
{
    if (columnState() == state) {
        return;
    }
    m_settings.setValue(QLatin1String(KeyColumnState), state);
    Q_EMIT columnStateChanged();
}

QString AppSettings::deviceIdentifier()
{
    QString identifier = m_settings.value(QLatin1String(KeyDeviceId)).toString();
    if (identifier.isEmpty()) {
        identifier = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings.setValue(QLatin1String(KeyDeviceId), identifier);
    }
    return identifier;
}

} // namespace kvault
