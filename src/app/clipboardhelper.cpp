#include "clipboardhelper.h"

#include "appsettings.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

namespace kvault {

namespace {

/**
 * Klipper and other KDE clipboard managers skip entries carrying this hint, so
 * copied passwords do not end up in a persistent history.
 */
void setClipboardSecret(const QString &text, bool secret)
{
    auto *mimeData = new QMimeData;
    mimeData->setText(text);
    if (secret) {
        mimeData->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));
    }
    QGuiApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);
}

} // namespace

ClipboardHelper::ClipboardHelper(QObject *parent)
    : QObject(parent)
    , m_settings(new AppSettings(this))
{
    m_countdown.setInterval(1000);
    connect(&m_countdown, &QTimer::timeout, this, [this]() {
        if (--m_secondsRemaining <= 0) {
            m_secondsRemaining = 0;
            m_countdown.stop();
            clearNow();
        }
        Q_EMIT secondsUntilClearChanged();
    });
}

void ClipboardHelper::copySecret(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    m_lastCopied = text;
    setClipboardSecret(text, true);
    scheduleClear();
}

void ClipboardHelper::copyPlain(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    // Not a secret, so leave it in the clipboard and let it into history.
    m_lastCopied.clear();
    m_countdown.stop();
    m_secondsRemaining = 0;
    setClipboardSecret(text, false);
    Q_EMIT secondsUntilClearChanged();
}

void ClipboardHelper::scheduleClear()
{
    const int seconds = m_settings->clipboardClearSeconds();
    if (seconds <= 0) {
        m_countdown.stop();
        m_secondsRemaining = 0;
    } else {
        m_secondsRemaining = seconds;
        m_countdown.start();
    }
    Q_EMIT secondsUntilClearChanged();
}

void ClipboardHelper::clearNow()
{
    m_countdown.stop();
    m_secondsRemaining = 0;

    // Only wipe the clipboard if it still holds our secret; the user may have
    // copied something else in the meantime.
    if (!m_lastCopied.isEmpty() && QGuiApplication::clipboard()->text() == m_lastCopied) {
        QGuiApplication::clipboard()->clear(QClipboard::Clipboard);
    }
    m_lastCopied.clear();
    Q_EMIT secondsUntilClearChanged();
}

} // namespace kvault
