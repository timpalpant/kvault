#include "totpcontroller.h"

#include <QDateTime>

namespace kvault {

TotpController::TotpController(QObject *parent)
    : QObject(parent)
{
    // A one-second tick is enough for both the code and the countdown ring.
    m_timer.setInterval(1000);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &TotpController::tick);
}

void TotpController::setSeed(const QString &seed)
{
    if (m_seed == seed) {
        return;
    }
    m_seed = seed;
    m_totp = Totp::parse(seed);

    if (m_totp.isValid()) {
        m_timer.start();
    } else {
        m_timer.stop();
    }

    Q_EMIT seedChanged();
    Q_EMIT tick();
}

QString TotpController::code() const
{
    return m_totp.isValid() ? m_totp.code(QDateTime::currentSecsSinceEpoch()) : QString();
}

QString TotpController::formattedCode() const
{
    const QString value = code();
    if (value.size() < 6) {
        return value;
    }
    // Split down the middle, which reads well for both six and eight digits.
    const int half = int(value.size()) / 2;
    return value.left(half) + QLatin1Char(' ') + value.mid(half);
}

int TotpController::secondsRemaining() const
{
    return m_totp.isValid() ? m_totp.secondsRemaining(QDateTime::currentSecsSinceEpoch()) : 0;
}

qreal TotpController::progress() const
{
    const int total = period();
    return total > 0 ? qreal(secondsRemaining()) / qreal(total) : 0.0;
}

} // namespace kvault
