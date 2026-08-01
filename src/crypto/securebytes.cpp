#include "securebytes.h"

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <QRandomGenerator>

namespace kvault {

void secureZero(void *data, std::size_t len) noexcept
{
    if (data && len) {
        OPENSSL_cleanse(data, len);
    }
}

SecureBytes::SecureBytes(const void *data, qsizetype len)
{
    if (data && len > 0) {
        const auto *p = static_cast<const uint8_t *>(data);
        m_data.assign(p, p + len);
    }
}

SecureBytes::SecureBytes(const QByteArray &data)
    : SecureBytes(data.constData(), data.size())
{}

SecureBytes::SecureBytes(qsizetype len)
{
    if (len > 0) {
        m_data.resize(size_t(len));
    }
}

SecureBytes SecureBytes::fromString(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    SecureBytes out(utf8.constData(), utf8.size());
    // The temporary QByteArray still holds a copy; wipe what we can reach.
    secureZero(const_cast<char *>(utf8.constData()), size_t(utf8.size()));
    return out;
}

SecureBytes SecureBytes::random(qsizetype len)
{
    SecureBytes out(len);
    if (len > 0 && RAND_bytes(out.data(), int(len)) != 1) {
        // Never fall back to a weaker source: a silent downgrade here would
        // compromise every key we generate.
        qFatal("kvault: RAND_bytes() failed, refusing to generate key material");
    }
    return out;
}

void SecureBytes::clear()
{
    secureZero(m_data.data(), m_data.size());
    m_data.clear();
    m_data.shrink_to_fit();
}

void SecureBytes::append(const void *data, qsizetype len)
{
    if (data && len > 0) {
        const auto *p = static_cast<const uint8_t *>(data);
        m_data.insert(m_data.end(), p, p + len);
    }
}

void SecureBytes::append(const SecureBytes &other)
{
    append(other.constData(), other.size());
}

SecureBytes SecureBytes::mid(qsizetype pos, qsizetype len) const
{
    if (pos < 0 || pos >= size()) {
        return {};
    }
    const qsizetype available = size() - pos;
    const qsizetype count = (len < 0 || len > available) ? available : len;
    return SecureBytes(m_data.data() + pos, count);
}

QByteArray SecureBytes::toByteArray() const
{
    return QByteArray(reinterpret_cast<const char *>(m_data.data()), qsizetype(m_data.size()));
}

QString SecureBytes::toString() const
{
    return QString::fromUtf8(reinterpret_cast<const char *>(m_data.data()), qsizetype(m_data.size()));
}

bool SecureBytes::operator==(const SecureBytes &other) const
{
    if (size() != other.size()) {
        return false;
    }
    if (isEmpty()) {
        return true;
    }
    return CRYPTO_memcmp(m_data.data(), other.m_data.data(), m_data.size()) == 0;
}

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    if (a.isEmpty()) {
        return true;
    }
    return CRYPTO_memcmp(a.constData(), b.constData(), size_t(a.size())) == 0;
}

} // namespace kvault
