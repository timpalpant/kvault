#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>

namespace kvault {

class AppSettings;

/**
 * Copies secrets to the clipboard and clears them again after a timeout.
 *
 * Copied secrets are tagged so that KDE's clipboard manager does not add them
 * to its history.
 */
class ClipboardHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int secondsUntilClear READ secondsUntilClear NOTIFY secondsUntilClearChanged)

public:
    explicit ClipboardHelper(QObject *parent = nullptr);

    /// Copy a secret; it will be cleared after the configured delay.
    Q_INVOKABLE void copySecret(const QString &text);
    /// Copy something non-sensitive, such as a username or URL. Not auto-cleared.
    Q_INVOKABLE void copyPlain(const QString &text);
    Q_INVOKABLE void clearNow();

    int secondsUntilClear() const { return m_secondsRemaining; }

Q_SIGNALS:
    void secondsUntilClearChanged();
    void copied(const QString &description);

private:
    void scheduleClear();

    AppSettings *m_settings;
    QTimer m_countdown;
    int m_secondsRemaining = 0;
    /// Only clear if the clipboard still holds what we put there.
    QString m_lastCopied;
};

} // namespace kvault
