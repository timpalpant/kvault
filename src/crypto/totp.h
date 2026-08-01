#pragma once

#include <QString>

namespace kvault {

/**
 * A parsed TOTP configuration.
 *
 * Bitwarden stores either a bare base32 secret or a full otpauth:// URI in the
 * login's "totp" field. Steam guard codes use the otpauth://totp/...&encoder=steam
 * form or the legacy "steam://<secret>" prefix.
 */
class Totp
{
public:
    enum class Algorithm {
        Sha1,
        Sha256,
        Sha512,
    };

    static Totp parse(const QString &keyOrUri);

    bool isValid() const { return m_valid; }
    int period() const { return m_period; }
    int digits() const { return m_digits; }
    bool isSteam() const { return m_steam; }
    QString issuer() const { return m_issuer; }
    QString accountName() const { return m_account; }

    /// The code for the interval containing @p unixSeconds.
    QString code(qint64 unixSeconds) const;
    /// Seconds until the current code expires.
    int secondsRemaining(qint64 unixSeconds) const;

private:
    QByteArray m_secret;
    Algorithm m_algorithm = Algorithm::Sha1;
    int m_period = 30;
    int m_digits = 6;
    bool m_steam = false;
    bool m_valid = false;
    QString m_issuer;
    QString m_account;
};

/// Decode an RFC 4648 base32 string, ignoring padding, spaces and case.
QByteArray base32Decode(const QString &input);

} // namespace kvault
