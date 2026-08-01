#include "folder.h"

#include "cipher.h"
#include "crypto/encstring.h"

namespace kvault {

std::optional<Folder> Folder::fromEncryptedJson(const QJsonObject &json, const SymmetricKey &userKey)
{
    Folder folder;
    folder.id = json.value(QStringLiteral("id")).toString();
    folder.revisionDate = parseBitwardenDate(json.value(QStringLiteral("revisionDate")).toString());

    const EncString name = EncString::parse(json.value(QStringLiteral("name")).toString());
    if (name.isNull()) {
        return std::nullopt;
    }
    const auto plain = name.decryptString(userKey);
    if (!plain) {
        return std::nullopt;
    }
    folder.name = *plain;
    return folder;
}

QJsonObject Folder::toEncryptedJson(const SymmetricKey &userKey) const
{
    const EncString encrypted = EncString::encryptString(name, userKey);
    if (encrypted.isNull()) {
        return {};
    }
    return QJsonObject{{QStringLiteral("name"), encrypted.toString()}};
}

} // namespace kvault
