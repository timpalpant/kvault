#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>

namespace kvault {

/// Presentation helpers shared by the QML views.
class IconHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit IconHelper(QObject *parent = nullptr);

    /// Freedesktop icon name for a cipher type.
    Q_INVOKABLE QString iconForType(int cipherType) const;
    Q_INVOKABLE QString labelForType(int cipherType) const;

    /**
     * The letter shown in an item's avatar.
     * Uses the item name, falling back to its host name.
     */
    Q_INVOKABLE QString avatarLetter(const QString &name) const;

    /**
     * A stable color for an item, derived from its name.
     * Deliberately local: no favicons are fetched, which would leak the
     * contents of the vault to third-party servers.
     */
    Q_INVOKABLE QColor avatarColor(const QString &name) const;

    /// Strip the scheme and path so a URI reads as a domain in the list.
    Q_INVOKABLE QString prettyHost(const QString &uri) const;

    Q_INVOKABLE QString formatFileSize(qint64 bytes) const;

    /**
     * Strength score (0-4) for an arbitrary password.
     *
     * Lives here rather than on PasswordGenerator because that type is
     * instantiated per view, and QML cannot call a static method on it.
     */
    Q_INVOKABLE int passwordStrength(const QString &password) const;
};

} // namespace kvault
