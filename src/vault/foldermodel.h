#pragma once

#include "model/folder.h"

#include <QAbstractListModel>
#include <QQmlEngine>

namespace kvault {

/// The vault's folders, with item counts for the sidebar.
class FolderModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Obtained from VaultManager")

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ItemCountRole,
    };
    Q_ENUM(Roles)

    explicit FolderModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFolders(QList<Folder> folders);
    void setItemCounts(const QHash<QString, int> &counts);
    void clear();

    Q_INVOKABLE QString folderName(const QString &folderId) const;
    const QList<Folder> &folders() const { return m_folders; }

private:
    QList<Folder> m_folders;
    QHash<QString, int> m_counts;
};

} // namespace kvault
