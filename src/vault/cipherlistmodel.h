#pragma once

#include "model/cipher.h"

#include <QAbstractListModel>
#include <QQmlEngine>

namespace kvault {

/// The unlocked vault's items, exposed to QML.
class CipherListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Obtained from VaultManager")

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SubtitleRole,
        TypeRole,
        FavoriteRole,
        FolderIdRole,
        OrganizationIdRole,
        HasTotpRole,
        InTrashRole,
        AttachmentCountRole,
        DecryptionFailedRole,
        PrimaryUriRole,
        RevisionDateRole,
    };
    Q_ENUM(Roles)

    explicit CipherListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCiphers(QList<Cipher> ciphers);
    void clear();

    /// Insert or replace a single item without reloading everything.
    void upsert(const Cipher &cipher);
    void removeById(const QString &id);

    const Cipher *cipherById(const QString &id) const;
    const QList<Cipher> &ciphers() const { return m_ciphers; }

    /// Lowercased haystack used by the search filter.
    QString searchIndexAt(int row) const;

    int countInFolder(const QString &folderId) const;
    int favoriteCount() const;
    int trashCount() const;
    int typeCount(CipherType type) const;

Q_SIGNALS:
    void countsChanged();

private:
    static QString buildSearchIndex(const Cipher &cipher);
    void rebuildIndex();

    QList<Cipher> m_ciphers;
    QList<QString> m_searchIndex;
    QHash<QString, int> m_rowById;
};

} // namespace kvault
