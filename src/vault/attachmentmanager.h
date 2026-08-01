#pragma once

#include <QObject>
#include <QQmlEngine>

namespace kvault {

class ApiClient;
class VaultKeys;
class CipherListModel;

/**
 * Downloads and decrypts item attachments.
 *
 * Attachment blobs are encrypted with their own key, which is itself wrapped by
 * the item's key, so decryption needs both the vault keys and the item.
 */
class AttachmentManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Obtained from VaultManager")

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

public:
    AttachmentManager(ApiClient *api, const VaultKeys *keys, const CipherListModel *ciphers, QObject *parent = nullptr);

    /**
     * Fetch an attachment and write the plaintext to @p destinationPath.
     * Reports the outcome through downloadFinished().
     */
    Q_INVOKABLE void download(const QString &cipherId, const QString &attachmentId, const QUrl &destinationPath);

    bool isBusy() const { return m_activeDownloads > 0; }

Q_SIGNALS:
    void busyChanged();
    void downloadFinished(const QString &attachmentId, bool success, const QString &message);

private:
    void fetchAndDecrypt(const QString &cipherId, const QString &attachmentId, const QString &url, const QString &localPath);
    void finish(const QString &attachmentId, bool success, const QString &message);

    ApiClient *m_api;
    const VaultKeys *m_keys;
    const CipherListModel *m_ciphers;
    int m_activeDownloads = 0;
};

} // namespace kvault
