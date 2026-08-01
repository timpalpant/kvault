#pragma once

#include "model/cipher.h"

#include <QQmlEngine>
#include <QSortFilterProxyModel>

namespace kvault {

class CipherListModel;

/// Search and sidebar filtering over CipherListModel.
class CipherFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Obtained from VaultManager")

    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(Scope scope READ scope WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(QString folderId READ folderId WRITE setFolderId NOTIFY folderIdChanged)
    Q_PROPERTY(int cipherType READ cipherType WRITE setCipherType NOTIFY cipherTypeChanged)

public:
    enum Scope {
        AllItems,
        Favorites,
        Folder,
        NoFolder,
        Type,
        Trash,
    };
    Q_ENUM(Scope)

    explicit CipherFilterProxyModel(QObject *parent = nullptr);

    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

    Scope scope() const { return m_scope; }
    void setScope(Scope scope);

    QString folderId() const { return m_folderId; }
    void setFolderId(const QString &folderId);

    int cipherType() const { return m_cipherType; }
    void setCipherType(int type);

Q_SIGNALS:
    void searchTextChanged();
    void scopeChanged();
    void folderIdChanged();
    void cipherTypeChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    CipherListModel *sourceCipherModel() const;

    QString m_searchText;
    QStringList m_searchTerms;
    Scope m_scope = AllItems;
    QString m_folderId;
    int m_cipherType = int(CipherType::Login);
};

} // namespace kvault
