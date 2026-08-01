#pragma once

#include "model/cipher.h"

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

namespace kvault {

/**
 * A mutable view of one item, bound directly to the edit form.
 *
 * All properties share a single `changed` signal. QML re-evaluates a few extra
 * bindings as a result, which is far cheaper than maintaining ~40 signals.
 */
class CipherEditor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Obtained from VaultManager")

    Q_PROPERTY(bool isNew READ isNew NOTIFY changed)
    Q_PROPERTY(QString cipherId MEMBER m_id NOTIFY changed)
    Q_PROPERTY(int type MEMBER m_type NOTIFY changed)
    Q_PROPERTY(QString name MEMBER m_name NOTIFY changed)
    Q_PROPERTY(QString notes MEMBER m_notes NOTIFY changed)
    Q_PROPERTY(bool favorite MEMBER m_favorite NOTIFY changed)
    Q_PROPERTY(QString folderId MEMBER m_folderId NOTIFY changed)
    Q_PROPERTY(int reprompt MEMBER m_reprompt NOTIFY changed)
    Q_PROPERTY(QString organizationId READ organizationId CONSTANT)

    // Login
    Q_PROPERTY(QString username MEMBER m_username NOTIFY changed)
    Q_PROPERTY(QString password MEMBER m_password NOTIFY changed)
    Q_PROPERTY(QString totp MEMBER m_totp NOTIFY changed)
    /// List of {uri, match} maps.
    Q_PROPERTY(QVariantList uris MEMBER m_uris NOTIFY changed)

    // Card
    Q_PROPERTY(QString cardholderName MEMBER m_cardholderName NOTIFY changed)
    Q_PROPERTY(QString cardBrand MEMBER m_cardBrand NOTIFY changed)
    Q_PROPERTY(QString cardNumber MEMBER m_cardNumber NOTIFY changed)
    Q_PROPERTY(QString cardExpMonth MEMBER m_cardExpMonth NOTIFY changed)
    Q_PROPERTY(QString cardExpYear MEMBER m_cardExpYear NOTIFY changed)
    Q_PROPERTY(QString cardCode MEMBER m_cardCode NOTIFY changed)

    // Identity
    Q_PROPERTY(QString identityTitle MEMBER m_identityTitle NOTIFY changed)
    Q_PROPERTY(QString firstName MEMBER m_firstName NOTIFY changed)
    Q_PROPERTY(QString middleName MEMBER m_middleName NOTIFY changed)
    Q_PROPERTY(QString lastName MEMBER m_lastName NOTIFY changed)
    Q_PROPERTY(QString address1 MEMBER m_address1 NOTIFY changed)
    Q_PROPERTY(QString address2 MEMBER m_address2 NOTIFY changed)
    Q_PROPERTY(QString address3 MEMBER m_address3 NOTIFY changed)
    Q_PROPERTY(QString city MEMBER m_city NOTIFY changed)
    Q_PROPERTY(QString state MEMBER m_state NOTIFY changed)
    Q_PROPERTY(QString postalCode MEMBER m_postalCode NOTIFY changed)
    Q_PROPERTY(QString country MEMBER m_country NOTIFY changed)
    Q_PROPERTY(QString company MEMBER m_company NOTIFY changed)
    Q_PROPERTY(QString email MEMBER m_email NOTIFY changed)
    Q_PROPERTY(QString phone MEMBER m_phone NOTIFY changed)
    Q_PROPERTY(QString ssn MEMBER m_ssn NOTIFY changed)
    Q_PROPERTY(QString identityUsername MEMBER m_identityUsername NOTIFY changed)
    Q_PROPERTY(QString passportNumber MEMBER m_passportNumber NOTIFY changed)
    Q_PROPERTY(QString licenseNumber MEMBER m_licenseNumber NOTIFY changed)

    // SSH key
    Q_PROPERTY(QString sshPrivateKey MEMBER m_sshPrivateKey NOTIFY changed)
    Q_PROPERTY(QString sshPublicKey MEMBER m_sshPublicKey NOTIFY changed)
    Q_PROPERTY(QString sshFingerprint MEMBER m_sshFingerprint NOTIFY changed)

    /// List of {name, value, type} maps.
    Q_PROPERTY(QVariantList fields MEMBER m_fields NOTIFY changed)

public:
    explicit CipherEditor(QObject *parent = nullptr);

    bool isNew() const { return m_id.isEmpty(); }
    QString organizationId() const { return m_organizationId; }

    /// Start editing an existing item.
    void loadFrom(const Cipher &cipher);
    /// Start a new item of the given type.
    void reset(CipherType type, const QString &folderId = QString());

    /// Build the item to send to the server, preserving fields the form does not expose.
    Cipher toCipher() const;

    Q_INVOKABLE void addUri();
    Q_INVOKABLE void removeUri(int index);
    Q_INVOKABLE void setUri(int index, const QString &uri);
    Q_INVOKABLE void setUriMatch(int index, int match);

    Q_INVOKABLE void addField(int type);
    Q_INVOKABLE void removeField(int index);
    Q_INVOKABLE void setFieldName(int index, const QString &name);
    Q_INVOKABLE void setFieldValue(int index, const QString &value);
    Q_INVOKABLE void moveField(int from, int to);

Q_SIGNALS:
    void changed();

private:
    /// Everything the form does not touch, carried through unmodified.
    Cipher m_original;
    QString m_organizationId;

    QString m_id;
    int m_type = int(CipherType::Login);
    QString m_name;
    QString m_notes;
    bool m_favorite = false;
    QString m_folderId;
    int m_reprompt = 0;

    QString m_username;
    QString m_password;
    QString m_totp;
    QVariantList m_uris;

    QString m_cardholderName;
    QString m_cardBrand;
    QString m_cardNumber;
    QString m_cardExpMonth;
    QString m_cardExpYear;
    QString m_cardCode;

    QString m_identityTitle;
    QString m_firstName;
    QString m_middleName;
    QString m_lastName;
    QString m_address1;
    QString m_address2;
    QString m_address3;
    QString m_city;
    QString m_state;
    QString m_postalCode;
    QString m_country;
    QString m_company;
    QString m_email;
    QString m_phone;
    QString m_ssn;
    QString m_identityUsername;
    QString m_passportNumber;
    QString m_licenseNumber;

    QString m_sshPrivateKey;
    QString m_sshPublicKey;
    QString m_sshFingerprint;

    QVariantList m_fields;
};

} // namespace kvault
