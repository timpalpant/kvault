#pragma once

#include "crypto/symmetrickey.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace kvault {

/// A vault folder. Only the name is encrypted.
struct Folder {
    QString id;
    QString name;
    QDateTime revisionDate;

    static std::optional<Folder> fromEncryptedJson(const QJsonObject &json, const SymmetricKey &userKey);
    QJsonObject toEncryptedJson(const SymmetricKey &userKey) const;
};

} // namespace kvault
