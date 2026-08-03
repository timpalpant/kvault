#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSettings>

namespace kvault {

/// Persistent user preferences. Nothing secret is stored here.
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString lastEmail READ lastEmail WRITE setLastEmail NOTIFY lastEmailChanged)
    /// Minutes of inactivity before locking; 0 means never.
    Q_PROPERTY(int lockTimeoutMinutes READ lockTimeoutMinutes WRITE setLockTimeoutMinutes NOTIFY lockTimeoutMinutesChanged)
    Q_PROPERTY(bool lockOnMinimize READ lockOnMinimize WRITE setLockOnMinimize NOTIFY lockOnMinimizeChanged)
    /// Seconds before a copied secret is cleared; 0 means never.
    Q_PROPERTY(int clipboardClearSeconds READ clipboardClearSeconds WRITE setClipboardClearSeconds NOTIFY clipboardClearSecondsChanged)
    Q_PROPERTY(bool syncOnUnlock READ syncOnUnlock WRITE setSyncOnUnlock NOTIFY syncOnUnlockChanged)
    Q_PROPERTY(bool concealPasswords READ concealPasswords WRITE setConcealPasswords NOTIFY concealPasswordsChanged)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY closeToTrayChanged)
    /// Opaque column widths blob owned by Kirigami's ColumnView.
    Q_PROPERTY(QString columnState READ columnState WRITE setColumnState NOTIFY columnStateChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString serverUrl() const;
    void setServerUrl(const QString &url);

    QString lastEmail() const;
    void setLastEmail(const QString &email);

    int lockTimeoutMinutes() const;
    void setLockTimeoutMinutes(int minutes);

    bool lockOnMinimize() const;
    void setLockOnMinimize(bool lock);

    int clipboardClearSeconds() const;
    void setClipboardClearSeconds(int seconds);

    bool syncOnUnlock() const;
    void setSyncOnUnlock(bool sync);

    bool concealPasswords() const;
    void setConcealPasswords(bool conceal);

    bool closeToTray() const;
    void setCloseToTray(bool closeToTray);

    QString columnState() const;
    void setColumnState(const QString &state);

    /**
     * A stable per-installation id, sent to the server so it can recognize this
     * device and not demand new-device verification on every login.
     */
    QString deviceIdentifier();

Q_SIGNALS:
    void serverUrlChanged();
    void lastEmailChanged();
    void lockTimeoutMinutesChanged();
    void lockOnMinimizeChanged();
    void clipboardClearSecondsChanged();
    void syncOnUnlockChanged();
    void concealPasswordsChanged();
    void closeToTrayChanged();
    void columnStateChanged();

private:
    QSettings m_settings;
};

} // namespace kvault
