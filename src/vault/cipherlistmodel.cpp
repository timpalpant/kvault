#include "cipherlistmodel.h"

#include <QUrl>

namespace kvault {

CipherListModel::CipherListModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int CipherListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_ciphers.size());
}

QHash<int, QByteArray> CipherListModel::roleNames() const
{
    return {
        {IdRole, "cipherId"},
        {NameRole, "name"},
        {SubtitleRole, "subtitle"},
        {TypeRole, "type"},
        {FavoriteRole, "favorite"},
        {FolderIdRole, "folderId"},
        {OrganizationIdRole, "organizationId"},
        {HasTotpRole, "hasTotp"},
        {InTrashRole, "inTrash"},
        {AttachmentCountRole, "attachmentCount"},
        {DecryptionFailedRole, "decryptionFailed"},
        {PrimaryUriRole, "primaryUri"},
        {RevisionDateRole, "revisionDate"},
    };
}

QVariant CipherListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_ciphers.size()) {
        return {};
    }
    const Cipher &cipher = m_ciphers.at(index.row());

    switch (role) {
    case IdRole:
        return cipher.id;
    case NameRole:
        return cipher.name;
    case SubtitleRole:
        return cipher.subtitle();
    case TypeRole:
        return int(cipher.type);
    case FavoriteRole:
        return cipher.favorite;
    case FolderIdRole:
        return cipher.folderId;
    case OrganizationIdRole:
        return cipher.organizationId;
    case HasTotpRole:
        return cipher.hasTotp();
    case InTrashRole:
        return cipher.isInTrash();
    case AttachmentCountRole:
        return int(cipher.attachments.size());
    case DecryptionFailedRole:
        return cipher.decryptionFailed;
    case PrimaryUriRole:
        return cipher.uris.isEmpty() ? QString() : cipher.uris.first().uri;
    case RevisionDateRole:
        return cipher.revisionDate;
    default:
        return {};
    }
}

QString CipherListModel::buildSearchIndex(const Cipher &cipher)
{
    QStringList parts{cipher.name, cipher.subtitle(), cipher.username, cipher.email, cipher.identityUsername};
    for (const LoginUri &uri : cipher.uris) {
        parts << uri.uri;
        // Also index the bare host so "github" matches "https://github.com/login".
        const QString host = QUrl(uri.uri).host();
        if (!host.isEmpty()) {
            parts << host;
        }
    }
    for (const CustomField &field : cipher.fields) {
        // Values may be secrets, so only field names are searchable.
        parts << field.name;
    }
    parts.removeAll(QString());
    return parts.join(QLatin1Char('\n')).toLower();
}

void CipherListModel::rebuildIndex()
{
    m_searchIndex.clear();
    m_searchIndex.reserve(m_ciphers.size());
    m_rowById.clear();
    m_rowById.reserve(m_ciphers.size());

    for (int row = 0; row < m_ciphers.size(); ++row) {
        m_searchIndex.append(buildSearchIndex(m_ciphers.at(row)));
        m_rowById.insert(m_ciphers.at(row).id, row);
    }
}

void CipherListModel::setCiphers(QList<Cipher> ciphers)
{
    beginResetModel();
    m_ciphers = std::move(ciphers);
    rebuildIndex();
    endResetModel();
    Q_EMIT countsChanged();
}

void CipherListModel::clear()
{
    beginResetModel();
    m_ciphers.clear();
    m_searchIndex.clear();
    m_rowById.clear();
    endResetModel();
    Q_EMIT countsChanged();
}

void CipherListModel::upsert(const Cipher &cipher)
{
    const auto it = m_rowById.constFind(cipher.id);
    if (it != m_rowById.constEnd()) {
        const int row = it.value();
        m_ciphers[row] = cipher;
        m_searchIndex[row] = buildSearchIndex(cipher);
        const QModelIndex changed = index(row, 0);
        Q_EMIT dataChanged(changed, changed);
    } else {
        const int row = int(m_ciphers.size());
        beginInsertRows(QModelIndex(), row, row);
        m_ciphers.append(cipher);
        m_searchIndex.append(buildSearchIndex(cipher));
        m_rowById.insert(cipher.id, row);
        endInsertRows();
    }
    Q_EMIT countsChanged();
}

void CipherListModel::removeById(const QString &id)
{
    const auto it = m_rowById.constFind(id);
    if (it == m_rowById.constEnd()) {
        return;
    }
    const int row = it.value();

    beginRemoveRows(QModelIndex(), row, row);
    m_ciphers.removeAt(row);
    m_searchIndex.removeAt(row);
    endRemoveRows();

    // Rows after the removed one shifted, so the id map has to be rebuilt.
    m_rowById.clear();
    for (int i = 0; i < m_ciphers.size(); ++i) {
        m_rowById.insert(m_ciphers.at(i).id, i);
    }
    Q_EMIT countsChanged();
}

const Cipher *CipherListModel::cipherById(const QString &id) const
{
    const auto it = m_rowById.constFind(id);
    return it == m_rowById.constEnd() ? nullptr : &m_ciphers.at(it.value());
}

QString CipherListModel::searchIndexAt(int row) const
{
    return (row >= 0 && row < m_searchIndex.size()) ? m_searchIndex.at(row) : QString();
}

int CipherListModel::countInFolder(const QString &folderId) const
{
    int count = 0;
    for (const Cipher &cipher : m_ciphers) {
        if (!cipher.isInTrash() && cipher.folderId == folderId) {
            ++count;
        }
    }
    return count;
}

int CipherListModel::favoriteCount() const
{
    int count = 0;
    for (const Cipher &cipher : m_ciphers) {
        if (cipher.favorite && !cipher.isInTrash()) {
            ++count;
        }
    }
    return count;
}

int CipherListModel::trashCount() const
{
    int count = 0;
    for (const Cipher &cipher : m_ciphers) {
        if (cipher.isInTrash()) {
            ++count;
        }
    }
    return count;
}

int CipherListModel::typeCount(CipherType type) const
{
    int count = 0;
    for (const Cipher &cipher : m_ciphers) {
        if (cipher.type == type && !cipher.isInTrash()) {
            ++count;
        }
    }
    return count;
}

} // namespace kvault
