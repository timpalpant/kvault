#pragma once

#include "crypto/totp.h"

#include <QObject>
#include <QQmlEngine>
#include <QTimer>

namespace kvault {

/// Drives a live TOTP code and its countdown for the detail view.
class TotpController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    /// The raw value from the item: a base32 secret or an otpauth:// URI.
    Q_PROPERTY(QString seed READ seed WRITE setSeed NOTIFY seedChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY seedChanged)
    Q_PROPERTY(int period READ period NOTIFY seedChanged)
    Q_PROPERTY(QString code READ code NOTIFY tick)
    /// The code split into readable groups, e.g. "123 456".
    Q_PROPERTY(QString formattedCode READ formattedCode NOTIFY tick)
    Q_PROPERTY(int secondsRemaining READ secondsRemaining NOTIFY tick)
    /// 0.0-1.0, for the countdown ring.
    Q_PROPERTY(qreal progress READ progress NOTIFY tick)

public:
    explicit TotpController(QObject *parent = nullptr);

    QString seed() const { return m_seed; }
    void setSeed(const QString &seed);

    bool isValid() const { return m_totp.isValid(); }
    int period() const { return m_totp.period(); }
    QString code() const;
    QString formattedCode() const;
    int secondsRemaining() const;
    qreal progress() const;

Q_SIGNALS:
    void seedChanged();
    void tick();

private:
    QString m_seed;
    Totp m_totp;
    QTimer m_timer;
};

} // namespace kvault
