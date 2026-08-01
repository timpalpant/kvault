#include "cipher.h"

#include <QJsonArray>
#include <QLoggingCategory>
#include <QTimeZone>

Q_LOGGING_CATEGORY(KVAULT_MODEL, "kvault.model")

namespace kvault {

namespace {

/// Decrypt an optional EncString field. Missing/null stays empty without being an error.
QString decryptField(const QJsonValue &value, const SymmetricKey &key, bool *failed, bool *unsupported)
{
    if (!value.isString()) {
        return {};
    }
    const QString encoded = value.toString();
    if (encoded.isEmpty()) {
        return {};
    }
    const EncString enc = EncString::parse(encoded);
    if (enc.isNull()) {
        qCWarning(KVAULT_MODEL) << "Malformed EncString in cipher payload";
        *failed = true;
        return {};
    }
    if (enc.isUnsupportedFormat()) {
        *unsupported = true;
        *failed = true;
        return {};
    }
    const auto plain = enc.decryptString(key);
    if (!plain) {
        *failed = true;
        return {};
    }
    return *plain;
}

/// Encrypt a value, or JSON null when it is empty (which is what the server expects).
QJsonValue encryptField(const QString &value, const SymmetricKey &key)
{
    if (value.isEmpty()) {
        return QJsonValue(QJsonValue::Null);
    }
    const EncString enc = EncString::encryptString(value, key);
    if (enc.isNull()) {
        return QJsonValue(QJsonValue::Null);
    }
    return enc.toString();
}

QJsonValue dateOrNull(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return QJsonValue(QJsonValue::Null);
    }
    return formatBitwardenDate(dateTime);
}

QString maskedCardNumber(const QString &number)
{
    const QString digits = number.trimmed();
    if (digits.size() <= 4) {
        return digits;
    }
    return QStringLiteral("*") + digits.right(4);
}

} // namespace

QDateTime parseBitwardenDate(const QString &text)
{
    if (text.isEmpty()) {
        return {};
    }
    QString normalised = text;

    // The server emits up to seven fractional digits; Qt only accepts three.
    const qsizetype dot = normalised.indexOf(u'.');
    if (dot >= 0) {
        qsizetype end = dot + 1;
        while (end < normalised.size() && normalised.at(end).isDigit()) {
            ++end;
        }
        const qsizetype digits = end - dot - 1;
        if (digits > 3) {
            normalised.remove(dot + 4, digits - 3);
        }
    }

    QDateTime dateTime = QDateTime::fromString(normalised, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(normalised, Qt::ISODate);
    }
    if (dateTime.isValid() && dateTime.timeSpec() == Qt::LocalTime) {
        // No zone designator: the server always means UTC.
        dateTime.setTimeZone(QTimeZone::UTC);
    }
    return dateTime;
}

QString formatBitwardenDate(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return {};
    }
    return dateTime.toUTC().toString(Qt::ISODateWithMs);
}

QString Cipher::subtitle() const
{
    switch (type) {
    case CipherType::Login:
        return username;
    case CipherType::Card: {
        QStringList parts;
        if (!cardBrand.isEmpty()) {
            parts << cardBrand;
        }
        if (!cardNumber.isEmpty()) {
            parts << maskedCardNumber(cardNumber);
        }
        return parts.join(QStringLiteral(", "));
    }
    case CipherType::Identity: {
        QStringList parts;
        if (!firstName.isEmpty()) {
            parts << firstName;
        }
        if (!lastName.isEmpty()) {
            parts << lastName;
        }
        return parts.join(QLatin1Char(' '));
    }
    case CipherType::SshKey:
        return sshFingerprint;
    case CipherType::SecureNote:
        break;
    }
    return {};
}

std::optional<SymmetricKey> Cipher::resolveItemKey(const QString &wrappedKey, const SymmetricKey &containerKey)
{
    if (wrappedKey.isEmpty()) {
        return containerKey;
    }
    const EncString enc = EncString::parse(wrappedKey);
    if (enc.isNull()) {
        return std::nullopt;
    }
    auto material = enc.decrypt(containerKey);
    if (!material) {
        return std::nullopt;
    }
    SymmetricKey key(std::move(*material));
    if (!key.isValid()) {
        return std::nullopt;
    }
    return key;
}

std::optional<Cipher> Cipher::fromEncryptedJson(const QJsonObject &json, const SymmetricKey &containerKey)
{
    Cipher cipher;
    cipher.id = json.value(QStringLiteral("id")).toString();
    cipher.organizationId = json.value(QStringLiteral("organizationId")).toString();
    cipher.folderId = json.value(QStringLiteral("folderId")).toString();
    cipher.type = CipherType(json.value(QStringLiteral("type")).toInt(1));
    cipher.favorite = json.value(QStringLiteral("favorite")).toBool();
    cipher.reprompt = json.value(QStringLiteral("reprompt")).toInt(0);
    cipher.edit = json.value(QStringLiteral("edit")).toBool(true);
    cipher.viewPassword = json.value(QStringLiteral("viewPassword")).toBool(true);
    cipher.creationDate = parseBitwardenDate(json.value(QStringLiteral("creationDate")).toString());
    cipher.revisionDate = parseBitwardenDate(json.value(QStringLiteral("revisionDate")).toString());
    cipher.deletedDate = parseBitwardenDate(json.value(QStringLiteral("deletedDate")).toString());
    cipher.wrappedItemKey = json.value(QStringLiteral("key")).toString();

    const auto itemKey = resolveItemKey(cipher.wrappedItemKey, containerKey);
    if (!itemKey) {
        qCWarning(KVAULT_MODEL) << "Could not unwrap item key for cipher" << cipher.id;
        return std::nullopt;
    }
    const SymmetricKey &key = *itemKey;
    bool failed = false;
    bool unsupported = false;

    cipher.name = decryptField(json.value(QStringLiteral("name")), key, &failed, &unsupported);
    cipher.notes = decryptField(json.value(QStringLiteral("notes")), key, &failed, &unsupported);

    switch (cipher.type) {
    case CipherType::Login: {
        const QJsonObject login = json.value(QStringLiteral("login")).toObject();
        cipher.username = decryptField(login.value(QStringLiteral("username")), key, &failed, &unsupported);
        cipher.password = decryptField(login.value(QStringLiteral("password")), key, &failed, &unsupported);
        cipher.totp = decryptField(login.value(QStringLiteral("totp")), key, &failed, &unsupported);
        cipher.passwordRevisionDate = parseBitwardenDate(login.value(QStringLiteral("passwordRevisionDate")).toString());
        for (const QJsonValue &value : login.value(QStringLiteral("uris")).toArray()) {
            const QJsonObject uriObject = value.toObject();
            LoginUri uri;
            uri.uri = decryptField(uriObject.value(QStringLiteral("uri")), key, &failed, &unsupported);
            const QJsonValue match = uriObject.value(QStringLiteral("match"));
            uri.match = match.isDouble() ? match.toInt() : int(UriMatchType::Default);
            if (!uri.uri.isEmpty()) {
                cipher.uris.append(uri);
            }
        }
        break;
    }
    case CipherType::SecureNote: {
        const QJsonObject note = json.value(QStringLiteral("secureNote")).toObject();
        cipher.secureNoteType = note.value(QStringLiteral("type")).toInt(0);
        break;
    }
    case CipherType::Card: {
        const QJsonObject card = json.value(QStringLiteral("card")).toObject();
        cipher.cardholderName = decryptField(card.value(QStringLiteral("cardholderName")), key, &failed, &unsupported);
        cipher.cardBrand = decryptField(card.value(QStringLiteral("brand")), key, &failed, &unsupported);
        cipher.cardNumber = decryptField(card.value(QStringLiteral("number")), key, &failed, &unsupported);
        cipher.cardExpMonth = decryptField(card.value(QStringLiteral("expMonth")), key, &failed, &unsupported);
        cipher.cardExpYear = decryptField(card.value(QStringLiteral("expYear")), key, &failed, &unsupported);
        cipher.cardCode = decryptField(card.value(QStringLiteral("code")), key, &failed, &unsupported);
        break;
    }
    case CipherType::Identity: {
        const QJsonObject identity = json.value(QStringLiteral("identity")).toObject();
        cipher.identityTitle = decryptField(identity.value(QStringLiteral("title")), key, &failed, &unsupported);
        cipher.firstName = decryptField(identity.value(QStringLiteral("firstName")), key, &failed, &unsupported);
        cipher.middleName = decryptField(identity.value(QStringLiteral("middleName")), key, &failed, &unsupported);
        cipher.lastName = decryptField(identity.value(QStringLiteral("lastName")), key, &failed, &unsupported);
        cipher.address1 = decryptField(identity.value(QStringLiteral("address1")), key, &failed, &unsupported);
        cipher.address2 = decryptField(identity.value(QStringLiteral("address2")), key, &failed, &unsupported);
        cipher.address3 = decryptField(identity.value(QStringLiteral("address3")), key, &failed, &unsupported);
        cipher.city = decryptField(identity.value(QStringLiteral("city")), key, &failed, &unsupported);
        cipher.state = decryptField(identity.value(QStringLiteral("state")), key, &failed, &unsupported);
        cipher.postalCode = decryptField(identity.value(QStringLiteral("postalCode")), key, &failed, &unsupported);
        cipher.country = decryptField(identity.value(QStringLiteral("country")), key, &failed, &unsupported);
        cipher.company = decryptField(identity.value(QStringLiteral("company")), key, &failed, &unsupported);
        cipher.email = decryptField(identity.value(QStringLiteral("email")), key, &failed, &unsupported);
        cipher.phone = decryptField(identity.value(QStringLiteral("phone")), key, &failed, &unsupported);
        cipher.ssn = decryptField(identity.value(QStringLiteral("ssn")), key, &failed, &unsupported);
        cipher.identityUsername = decryptField(identity.value(QStringLiteral("username")), key, &failed, &unsupported);
        cipher.passportNumber = decryptField(identity.value(QStringLiteral("passportNumber")), key, &failed, &unsupported);
        cipher.licenseNumber = decryptField(identity.value(QStringLiteral("licenseNumber")), key, &failed, &unsupported);
        break;
    }
    case CipherType::SshKey: {
        const QJsonObject sshKey = json.value(QStringLiteral("sshKey")).toObject();
        cipher.sshPrivateKey = decryptField(sshKey.value(QStringLiteral("privateKey")), key, &failed, &unsupported);
        cipher.sshPublicKey = decryptField(sshKey.value(QStringLiteral("publicKey")), key, &failed, &unsupported);
        cipher.sshFingerprint = decryptField(sshKey.value(QStringLiteral("keyFingerprint")), key, &failed, &unsupported);
        break;
    }
    }

    for (const QJsonValue &value : json.value(QStringLiteral("fields")).toArray()) {
        const QJsonObject fieldObject = value.toObject();
        CustomField field;
        field.type = fieldObject.value(QStringLiteral("type")).toInt(0);
        field.name = decryptField(fieldObject.value(QStringLiteral("name")), key, &failed, &unsupported);
        field.value = decryptField(fieldObject.value(QStringLiteral("value")), key, &failed, &unsupported);
        const QJsonValue linkedId = fieldObject.value(QStringLiteral("linkedId"));
        field.linkedId = linkedId.isDouble() ? linkedId.toInt() : -1;
        cipher.fields.append(field);
    }

    for (const QJsonValue &value : json.value(QStringLiteral("attachments")).toArray()) {
        const QJsonObject attachmentObject = value.toObject();
        AttachmentInfo attachment;
        attachment.id = attachmentObject.value(QStringLiteral("id")).toString();
        attachment.url = attachmentObject.value(QStringLiteral("url")).toString();
        attachment.key = attachmentObject.value(QStringLiteral("key")).toString();
        attachment.size = attachmentObject.value(QStringLiteral("size")).toString().toLongLong();
        attachment.sizeName = attachmentObject.value(QStringLiteral("sizeName")).toString();
        attachment.fileName = decryptField(attachmentObject.value(QStringLiteral("fileName")), key, &failed, &unsupported);
        cipher.attachments.append(attachment);
    }

    for (const QJsonValue &value : json.value(QStringLiteral("passwordHistory")).toArray()) {
        const QJsonObject historyObject = value.toObject();
        PasswordHistoryEntry entry;
        entry.password = decryptField(historyObject.value(QStringLiteral("password")), key, &failed, &unsupported);
        entry.lastUsedDate = parseBitwardenDate(historyObject.value(QStringLiteral("lastUsedDate")).toString());
        cipher.passwordHistory.append(entry);
    }

    cipher.decryptionFailed = failed;
    cipher.usesUnsupportedEncryption = unsupported;
    return cipher;
}

QJsonObject Cipher::toEncryptedJson(const SymmetricKey &containerKey) const
{
    const auto itemKey = resolveItemKey(wrappedItemKey, containerKey);
    if (!itemKey) {
        return {};
    }
    const SymmetricKey &key = *itemKey;

    QJsonObject json;
    json.insert(QStringLiteral("type"), int(type));
    json.insert(QStringLiteral("name"), encryptField(name, key));
    json.insert(QStringLiteral("notes"), encryptField(notes, key));
    json.insert(QStringLiteral("favorite"), favorite);
    json.insert(QStringLiteral("reprompt"), reprompt);
    json.insert(QStringLiteral("folderId"), folderId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(folderId));
    json.insert(QStringLiteral("organizationId"), organizationId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(organizationId));
    json.insert(QStringLiteral("key"), wrappedItemKey.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(wrappedItemKey));
    if (revisionDate.isValid()) {
        json.insert(QStringLiteral("lastKnownRevisionDate"), formatBitwardenDate(revisionDate));
    }

    switch (type) {
    case CipherType::Login: {
        QJsonObject login;
        login.insert(QStringLiteral("username"), encryptField(username, key));
        login.insert(QStringLiteral("password"), encryptField(password, key));
        login.insert(QStringLiteral("totp"), encryptField(totp, key));
        login.insert(QStringLiteral("passwordRevisionDate"), dateOrNull(passwordRevisionDate));
        QJsonArray uriArray;
        for (const LoginUri &uri : uris) {
            if (uri.uri.isEmpty()) {
                continue;
            }
            QJsonObject uriObject;
            uriObject.insert(QStringLiteral("uri"), encryptField(uri.uri, key));
            uriObject.insert(QStringLiteral("match"), uri.match < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(uri.match));
            uriArray.append(uriObject);
        }
        login.insert(QStringLiteral("uris"), uriArray);
        json.insert(QStringLiteral("login"), login);
        break;
    }
    case CipherType::SecureNote: {
        json.insert(QStringLiteral("secureNote"), QJsonObject{{QStringLiteral("type"), secureNoteType}});
        break;
    }
    case CipherType::Card: {
        QJsonObject card;
        card.insert(QStringLiteral("cardholderName"), encryptField(cardholderName, key));
        card.insert(QStringLiteral("brand"), encryptField(cardBrand, key));
        card.insert(QStringLiteral("number"), encryptField(cardNumber, key));
        card.insert(QStringLiteral("expMonth"), encryptField(cardExpMonth, key));
        card.insert(QStringLiteral("expYear"), encryptField(cardExpYear, key));
        card.insert(QStringLiteral("code"), encryptField(cardCode, key));
        json.insert(QStringLiteral("card"), card);
        break;
    }
    case CipherType::Identity: {
        QJsonObject identity;
        identity.insert(QStringLiteral("title"), encryptField(identityTitle, key));
        identity.insert(QStringLiteral("firstName"), encryptField(firstName, key));
        identity.insert(QStringLiteral("middleName"), encryptField(middleName, key));
        identity.insert(QStringLiteral("lastName"), encryptField(lastName, key));
        identity.insert(QStringLiteral("address1"), encryptField(address1, key));
        identity.insert(QStringLiteral("address2"), encryptField(address2, key));
        identity.insert(QStringLiteral("address3"), encryptField(address3, key));
        identity.insert(QStringLiteral("city"), encryptField(city, key));
        identity.insert(QStringLiteral("state"), encryptField(state, key));
        identity.insert(QStringLiteral("postalCode"), encryptField(postalCode, key));
        identity.insert(QStringLiteral("country"), encryptField(country, key));
        identity.insert(QStringLiteral("company"), encryptField(company, key));
        identity.insert(QStringLiteral("email"), encryptField(email, key));
        identity.insert(QStringLiteral("phone"), encryptField(phone, key));
        identity.insert(QStringLiteral("ssn"), encryptField(ssn, key));
        identity.insert(QStringLiteral("username"), encryptField(identityUsername, key));
        identity.insert(QStringLiteral("passportNumber"), encryptField(passportNumber, key));
        identity.insert(QStringLiteral("licenseNumber"), encryptField(licenseNumber, key));
        json.insert(QStringLiteral("identity"), identity);
        break;
    }
    case CipherType::SshKey: {
        QJsonObject sshKey;
        sshKey.insert(QStringLiteral("privateKey"), encryptField(sshPrivateKey, key));
        sshKey.insert(QStringLiteral("publicKey"), encryptField(sshPublicKey, key));
        sshKey.insert(QStringLiteral("keyFingerprint"), encryptField(sshFingerprint, key));
        json.insert(QStringLiteral("sshKey"), sshKey);
        break;
    }
    }

    QJsonArray fieldArray;
    for (const CustomField &field : fields) {
        if (field.name.isEmpty() && field.value.isEmpty()) {
            continue;
        }
        QJsonObject fieldObject;
        fieldObject.insert(QStringLiteral("type"), field.type);
        fieldObject.insert(QStringLiteral("name"), encryptField(field.name, key));
        fieldObject.insert(QStringLiteral("value"), encryptField(field.value, key));
        fieldObject.insert(QStringLiteral("linkedId"), field.linkedId < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(field.linkedId));
        fieldArray.append(fieldObject);
    }
    json.insert(QStringLiteral("fields"), fieldArray);

    QJsonArray historyArray;
    for (const PasswordHistoryEntry &entry : passwordHistory) {
        if (entry.password.isEmpty()) {
            continue;
        }
        QJsonObject historyObject;
        historyObject.insert(QStringLiteral("password"), encryptField(entry.password, key));
        historyObject.insert(QStringLiteral("lastUsedDate"), dateOrNull(entry.lastUsedDate));
        historyArray.append(historyObject);
    }
    json.insert(QStringLiteral("passwordHistory"), historyArray);

    // Without this the server drops existing attachments on update.
    if (!attachments.isEmpty()) {
        QJsonObject attachmentMap;
        for (const AttachmentInfo &attachment : attachments) {
            if (attachment.id.isEmpty()) {
                continue;
            }
            attachmentMap.insert(attachment.id,
                                 QJsonObject{
                                     {QStringLiteral("fileName"), encryptField(attachment.fileName, key)},
                                     {QStringLiteral("key"), attachment.key.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(attachment.key)},
                                 });
        }
        json.insert(QStringLiteral("attachments2"), attachmentMap);
    }

    return json;
}

} // namespace kvault
