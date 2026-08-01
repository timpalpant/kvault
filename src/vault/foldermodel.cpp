#include "foldermodel.h"

#include <algorithm>

namespace kvault {

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_folders.size());
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return {
        {IdRole, "folderId"},
        {NameRole, "name"},
        {ItemCountRole, "itemCount"},
    };
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_folders.size()) {
        return {};
    }
    const Folder &folder = m_folders.at(index.row());
    switch (role) {
    case IdRole:
        return folder.id;
    case NameRole:
        return folder.name;
    case ItemCountRole:
        return m_counts.value(folder.id, 0);
    default:
        return {};
    }
}

void FolderModel::setFolders(QList<Folder> folders)
{
    std::sort(
        folders.begin(), folders.end(), [](const Folder &a, const Folder &b) { return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0; });

    beginResetModel();
    m_folders = std::move(folders);
    endResetModel();
}

void FolderModel::setItemCounts(const QHash<QString, int> &counts)
{
    m_counts = counts;
    if (!m_folders.isEmpty()) {
        Q_EMIT dataChanged(index(0, 0), index(int(m_folders.size()) - 1, 0), {ItemCountRole});
    }
}

void FolderModel::clear()
{
    beginResetModel();
    m_folders.clear();
    m_counts.clear();
    endResetModel();
}

QString FolderModel::folderName(const QString &folderId) const
{
    for (const Folder &folder : m_folders) {
        if (folder.id == folderId) {
            return folder.name;
        }
    }
    return {};
}

} // namespace kvault
