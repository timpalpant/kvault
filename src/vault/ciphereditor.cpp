#include "ciphereditor.h"

#include <QVariantMap>

namespace kvault {

namespace {

QVariantMap uriToMap(const LoginUri &uri)
{
    return {{QStringLiteral("uri"), uri.uri}, {QStringLiteral("match"), uri.match}};
}

QVariantMap fieldToMap(const CustomField &field)
{
    return {
        {QStringLiteral("name"), field.name},
        {QStringLiteral("value"), field.value},
        {QStringLiteral("type"), field.type},
        {QStringLiteral("linkedId"), field.linkedId},
    };
}

} // namespace

CipherEditor::CipherEditor(QObject *parent)
    : QObject(parent)
{}

void CipherEditor::reset(CipherType type, const QString &folderId)
{
    m_original = Cipher();
    m_original.type = type;
    m_organizationId.clear();

    m_id.clear();
    m_type = int(type);
    m_name.clear();
    m_notes.clear();
    m_favorite = false;
    m_folderId = folderId;
    m_reprompt = 0;

    m_username.clear();
    m_password.clear();
    m_totp.clear();
    m_uris.clear();

    m_cardholderName.clear();
    m_cardBrand.clear();
    m_cardNumber.clear();
    m_cardExpMonth.clear();
    m_cardExpYear.clear();
    m_cardCode.clear();

    m_identityTitle.clear();
    m_firstName.clear();
    m_middleName.clear();
    m_lastName.clear();
    m_address1.clear();
    m_address2.clear();
    m_address3.clear();
    m_city.clear();
    m_state.clear();
    m_postalCode.clear();
    m_country.clear();
    m_company.clear();
    m_email.clear();
    m_phone.clear();
    m_ssn.clear();
    m_identityUsername.clear();
    m_passportNumber.clear();
    m_licenseNumber.clear();

    m_sshPrivateKey.clear();
    m_sshPublicKey.clear();
    m_sshFingerprint.clear();

    m_fields.clear();

    // A new login starts with one empty URI row so there is something to type into.
    if (type == CipherType::Login) {
        m_uris.append(QVariantMap{{QStringLiteral("uri"), QString()}, {QStringLiteral("match"), int(UriMatchType::Default)}});
    }

    Q_EMIT changed();
}

void CipherEditor::loadFrom(const Cipher &cipher)
{
    m_original = cipher;
    m_organizationId = cipher.organizationId;

    m_id = cipher.id;
    m_type = int(cipher.type);
    m_name = cipher.name;
    m_notes = cipher.notes;
    m_favorite = cipher.favorite;
    m_folderId = cipher.folderId;
    m_reprompt = cipher.reprompt;

    m_username = cipher.username;
    m_password = cipher.password;
    m_totp = cipher.totp;
    m_uris.clear();
    for (const LoginUri &uri : cipher.uris) {
        m_uris.append(uriToMap(uri));
    }

    m_cardholderName = cipher.cardholderName;
    m_cardBrand = cipher.cardBrand;
    m_cardNumber = cipher.cardNumber;
    m_cardExpMonth = cipher.cardExpMonth;
    m_cardExpYear = cipher.cardExpYear;
    m_cardCode = cipher.cardCode;

    m_identityTitle = cipher.identityTitle;
    m_firstName = cipher.firstName;
    m_middleName = cipher.middleName;
    m_lastName = cipher.lastName;
    m_address1 = cipher.address1;
    m_address2 = cipher.address2;
    m_address3 = cipher.address3;
    m_city = cipher.city;
    m_state = cipher.state;
    m_postalCode = cipher.postalCode;
    m_country = cipher.country;
    m_company = cipher.company;
    m_email = cipher.email;
    m_phone = cipher.phone;
    m_ssn = cipher.ssn;
    m_identityUsername = cipher.identityUsername;
    m_passportNumber = cipher.passportNumber;
    m_licenseNumber = cipher.licenseNumber;

    m_sshPrivateKey = cipher.sshPrivateKey;
    m_sshPublicKey = cipher.sshPublicKey;
    m_sshFingerprint = cipher.sshFingerprint;

    m_fields.clear();
    for (const CustomField &field : cipher.fields) {
        m_fields.append(fieldToMap(field));
    }

    Q_EMIT changed();
}

Cipher CipherEditor::toCipher() const
{
    // Start from the original so attachments, item key, revision date and
    // password history survive an edit.
    Cipher cipher = m_original;

    cipher.id = m_id;
    cipher.type = CipherType(m_type);
    cipher.name = m_name;
    cipher.notes = m_notes;
    cipher.favorite = m_favorite;
    cipher.folderId = m_folderId;
    cipher.reprompt = m_reprompt;
    cipher.organizationId = m_organizationId;

    cipher.username = m_username;
    cipher.totp = m_totp;

    // Changing the password pushes the old one into history, as other clients do.
    if (m_password != m_original.password) {
        if (!m_original.password.isEmpty()) {
            cipher.passwordHistory.prepend(
                {m_original.password, m_original.passwordRevisionDate.isValid() ? m_original.passwordRevisionDate : QDateTime::currentDateTimeUtc()});
            // The server caps history at five entries.
            while (cipher.passwordHistory.size() > 5) {
                cipher.passwordHistory.removeLast();
            }
        }
        cipher.passwordRevisionDate = QDateTime::currentDateTimeUtc();
    }
    cipher.password = m_password;

    cipher.uris.clear();
    for (const QVariant &value : m_uris) {
        const QVariantMap map = value.toMap();
        const QString uri = map.value(QStringLiteral("uri")).toString().trimmed();
        if (uri.isEmpty()) {
            continue;
        }
        cipher.uris.append({uri, map.value(QStringLiteral("match"), int(UriMatchType::Default)).toInt()});
    }

    cipher.cardholderName = m_cardholderName;
    cipher.cardBrand = m_cardBrand;
    cipher.cardNumber = m_cardNumber;
    cipher.cardExpMonth = m_cardExpMonth;
    cipher.cardExpYear = m_cardExpYear;
    cipher.cardCode = m_cardCode;

    cipher.identityTitle = m_identityTitle;
    cipher.firstName = m_firstName;
    cipher.middleName = m_middleName;
    cipher.lastName = m_lastName;
    cipher.address1 = m_address1;
    cipher.address2 = m_address2;
    cipher.address3 = m_address3;
    cipher.city = m_city;
    cipher.state = m_state;
    cipher.postalCode = m_postalCode;
    cipher.country = m_country;
    cipher.company = m_company;
    cipher.email = m_email;
    cipher.phone = m_phone;
    cipher.ssn = m_ssn;
    cipher.identityUsername = m_identityUsername;
    cipher.passportNumber = m_passportNumber;
    cipher.licenseNumber = m_licenseNumber;

    cipher.sshPrivateKey = m_sshPrivateKey;
    cipher.sshPublicKey = m_sshPublicKey;
    cipher.sshFingerprint = m_sshFingerprint;

    cipher.fields.clear();
    for (const QVariant &value : m_fields) {
        const QVariantMap map = value.toMap();
        CustomField field;
        field.name = map.value(QStringLiteral("name")).toString();
        field.value = map.value(QStringLiteral("value")).toString();
        field.type = map.value(QStringLiteral("type"), int(FieldType::Text)).toInt();
        field.linkedId = map.value(QStringLiteral("linkedId"), -1).toInt();
        if (field.name.isEmpty() && field.value.isEmpty()) {
            continue;
        }
        cipher.fields.append(field);
    }

    return cipher;
}

void CipherEditor::addUri()
{
    m_uris.append(QVariantMap{{QStringLiteral("uri"), QString()}, {QStringLiteral("match"), int(UriMatchType::Default)}});
    Q_EMIT changed();
}

void CipherEditor::removeUri(int index)
{
    if (index < 0 || index >= m_uris.size()) {
        return;
    }
    m_uris.removeAt(index);
    Q_EMIT changed();
}

void CipherEditor::setUri(int index, const QString &uri)
{
    if (index < 0 || index >= m_uris.size()) {
        return;
    }
    QVariantMap map = m_uris.at(index).toMap();
    if (map.value(QStringLiteral("uri")).toString() == uri) {
        return;
    }
    map.insert(QStringLiteral("uri"), uri);
    m_uris[index] = map;
    Q_EMIT changed();
}

void CipherEditor::setUriMatch(int index, int match)
{
    if (index < 0 || index >= m_uris.size()) {
        return;
    }
    QVariantMap map = m_uris.at(index).toMap();
    if (map.value(QStringLiteral("match")).toInt() == match) {
        return;
    }
    map.insert(QStringLiteral("match"), match);
    m_uris[index] = map;
    Q_EMIT changed();
}

void CipherEditor::addField(int type)
{
    m_fields.append(QVariantMap{
        {QStringLiteral("name"), QString()},
        {QStringLiteral("value"), type == int(FieldType::Boolean) ? QStringLiteral("false") : QString()},
        {QStringLiteral("type"), type},
        {QStringLiteral("linkedId"), -1},
    });
    Q_EMIT changed();
}

void CipherEditor::removeField(int index)
{
    if (index < 0 || index >= m_fields.size()) {
        return;
    }
    m_fields.removeAt(index);
    Q_EMIT changed();
}

void CipherEditor::setFieldName(int index, const QString &name)
{
    if (index < 0 || index >= m_fields.size()) {
        return;
    }
    QVariantMap map = m_fields.at(index).toMap();
    if (map.value(QStringLiteral("name")).toString() == name) {
        return;
    }
    map.insert(QStringLiteral("name"), name);
    m_fields[index] = map;
    Q_EMIT changed();
}

void CipherEditor::setFieldValue(int index, const QString &value)
{
    if (index < 0 || index >= m_fields.size()) {
        return;
    }
    QVariantMap map = m_fields.at(index).toMap();
    if (map.value(QStringLiteral("value")).toString() == value) {
        return;
    }
    map.insert(QStringLiteral("value"), value);
    m_fields[index] = map;
    Q_EMIT changed();
}

void CipherEditor::moveField(int from, int to)
{
    if (from < 0 || from >= m_fields.size() || to < 0 || to >= m_fields.size() || from == to) {
        return;
    }
    m_fields.move(from, to);
    Q_EMIT changed();
}

} // namespace kvault
