#include "attachmentmanager.h"

#include "api/apiclient.h"
#include "cipherlistmodel.h"
#include "crypto/encstring.h"
#include "vaultkeys.h"

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QUrl>

#include <KLocalizedString>

Q_DECLARE_LOGGING_CATEGORY(KVAULT_VAULT)

namespace kvault {

AttachmentManager::AttachmentManager(ApiClient *api, const VaultKeys *keys, const CipherListModel *ciphers, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_keys(keys)
    , m_ciphers(ciphers)
{}

void AttachmentManager::finish(const QString &attachmentId, bool success, const QString &message)
{
    if (m_activeDownloads > 0) {
        --m_activeDownloads;
        Q_EMIT busyChanged();
    }
    Q_EMIT downloadFinished(attachmentId, success, message);
}

void AttachmentManager::download(const QString &cipherId, const QString &attachmentId, const QUrl &destinationPath)
{
    const QString localPath = destinationPath.isLocalFile() ? destinationPath.toLocalFile() : destinationPath.toString();
    if (localPath.isEmpty()) {
        Q_EMIT downloadFinished(attachmentId, false, i18n("No destination was chosen."));
        return;
    }

    ++m_activeDownloads;
    Q_EMIT busyChanged();

    // The URL in the sync payload is pre-signed and expires, so always ask the
    // server for a fresh one rather than reusing the cached value.
    m_api->attachmentInfo(cipherId, attachmentId, [this, cipherId, attachmentId, localPath](const ApiResponse &response) {
        if (!response.ok) {
            finish(attachmentId, false, response.errorMessage);
            return;
        }
        const QString url = response.json.value(QStringLiteral("url")).toString();
        if (url.isEmpty()) {
            finish(attachmentId, false, i18n("The server did not return a download link."));
            return;
        }
        fetchAndDecrypt(cipherId, attachmentId, url, localPath);
    });
}

void AttachmentManager::fetchAndDecrypt(const QString &cipherId, const QString &attachmentId, const QString &url, const QString &localPath)
{
    m_api->downloadUrl(url, [this, cipherId, attachmentId, localPath](const ApiResponse &response) {
        if (!response.ok) {
            finish(attachmentId, false, response.errorMessage);
            return;
        }

        const Cipher *cipher = m_ciphers->cipherById(cipherId);
        if (!cipher) {
            finish(attachmentId, false, i18n("The item is no longer available."));
            return;
        }

        const AttachmentInfo *attachment = nullptr;
        for (const AttachmentInfo &candidate : cipher->attachments) {
            if (candidate.id == attachmentId) {
                attachment = &candidate;
                break;
            }
        }
        if (!attachment) {
            finish(attachmentId, false, i18n("The attachment is no longer part of this item."));
            return;
        }

        if (!m_keys->hasKeyForOrganization(cipher->organizationId)) {
            finish(attachmentId, false, i18n("The vault is locked."));
            return;
        }
        const SymmetricKey &containerKey = m_keys->keyForOrganization(cipher->organizationId);

        // The attachment key is wrapped by the item's key, which may itself be
        // wrapped. Older attachments have no key of their own and use the
        // item key directly.
        const auto itemKey = Cipher::resolveItemKey(cipher->wrappedItemKey, containerKey);
        if (!itemKey) {
            finish(attachmentId, false, i18n("Could not unlock this item."));
            return;
        }

        SymmetricKey attachmentKey = *itemKey;
        if (!attachment->key.isEmpty()) {
            const EncString wrapped = EncString::parse(attachment->key);
            auto material = wrapped.decrypt(*itemKey);
            if (!material) {
                finish(attachmentId, false, i18n("Could not decrypt the attachment key."));
                return;
            }
            attachmentKey = SymmetricKey(std::move(*material));
        }

        const EncString blob = EncString::fromPackedBuffer(response.raw);
        if (blob.isNull()) {
            finish(attachmentId, false, i18n("The downloaded attachment is malformed."));
            return;
        }
        const auto plaintext = blob.decrypt(attachmentKey);
        if (!plaintext) {
            finish(attachmentId, false, i18n("The attachment failed its integrity check."));
            return;
        }

        QSaveFile file(localPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            finish(attachmentId, false, i18n("Could not write to %1.", localPath));
            return;
        }
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        const QByteArray data = plaintext->toByteArray();
        if (file.write(data) != data.size() || !file.commit()) {
            finish(attachmentId, false, i18n("Could not write to %1.", localPath));
            return;
        }

        finish(attachmentId, true, i18n("Saved to %1", QFileInfo(localPath).fileName()));
    });
}

} // namespace kvault
