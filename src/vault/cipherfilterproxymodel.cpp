#include "cipherfilterproxymodel.h"

#include "cipherlistmodel.h"

namespace kvault {

CipherFilterProxyModel::CipherFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortLocaleAware(true);
    sort(0);
}

CipherListModel *CipherFilterProxyModel::sourceCipherModel() const
{
    return qobject_cast<CipherListModel *>(sourceModel());
}

void CipherFilterProxyModel::setSearchText(const QString &text)
{
    if (m_searchText == text) {
        return;
    }
    beginFilterChange();
    m_searchText = text;
    // Every whitespace-separated term must match, so "git oct" finds the GitHub
    // login for octocat regardless of field order.
    m_searchTerms = text.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT searchTextChanged();
}

void CipherFilterProxyModel::setScope(Scope scope)
{
    if (m_scope == scope) {
        return;
    }
    beginFilterChange();
    m_scope = scope;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT scopeChanged();
}

void CipherFilterProxyModel::setFolderId(const QString &folderId)
{
    if (m_folderId == folderId) {
        return;
    }
    beginFilterChange();
    m_folderId = folderId;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT folderIdChanged();
}

void CipherFilterProxyModel::setCipherType(int type)
{
    if (m_cipherType == type) {
        return;
    }
    beginFilterChange();
    m_cipherType = type;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT cipherTypeChanged();
}

bool CipherFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    CipherListModel *model = sourceCipherModel();
    if (!model) {
        return false;
    }
    const QModelIndex index = model->index(sourceRow, 0, sourceParent);
    const bool inTrash = index.data(CipherListModel::InTrashRole).toBool();

    // Trash is a separate world: its items never show up anywhere else.
    if (m_scope == Trash) {
        if (!inTrash) {
            return false;
        }
    } else if (inTrash) {
        return false;
    }

    switch (m_scope) {
    case AllItems:
    case Trash:
        break;
    case Favorites:
        if (!index.data(CipherListModel::FavoriteRole).toBool()) {
            return false;
        }
        break;
    case Folder:
        if (index.data(CipherListModel::FolderIdRole).toString() != m_folderId) {
            return false;
        }
        break;
    case NoFolder:
        if (!index.data(CipherListModel::FolderIdRole).toString().isEmpty()) {
            return false;
        }
        break;
    case Type:
        if (index.data(CipherListModel::TypeRole).toInt() != m_cipherType) {
            return false;
        }
        break;
    }

    if (m_searchTerms.isEmpty()) {
        return true;
    }
    const QString haystack = model->searchIndexAt(sourceRow);
    for (const QString &term : m_searchTerms) {
        if (!haystack.contains(term)) {
            return false;
        }
    }
    return true;
}

bool CipherFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // Favourites float to the top, except in trash where that ordering is noise.
    if (m_scope != Trash) {
        const bool leftFavorite = left.data(CipherListModel::FavoriteRole).toBool();
        const bool rightFavorite = right.data(CipherListModel::FavoriteRole).toBool();
        if (leftFavorite != rightFavorite) {
            return leftFavorite;
        }
    }

    const QString leftName = left.data(CipherListModel::NameRole).toString();
    const QString rightName = right.data(CipherListModel::NameRole).toString();
    const int comparison = QString::compare(leftName, rightName, Qt::CaseInsensitive);
    if (comparison != 0) {
        return comparison < 0;
    }
    // Stable tie-break so equal names do not shuffle between refreshes.
    return left.data(CipherListModel::IdRole).toString() < right.data(CipherListModel::IdRole).toString();
}

} // namespace kvault
