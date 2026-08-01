#pragma once

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace kvault {

/**
 * Allocator that wipes memory before handing it back to the system.
 *
 * std::vector may reallocate as it grows, which would otherwise leave copies of
 * key material scattered across the heap. Wiping in deallocate() covers both the
 * growth path and normal destruction.
 */
template<typename T> struct CleansingAllocator {
    using value_type = T;

    CleansingAllocator() noexcept = default;
    template<typename U> constexpr CleansingAllocator(const CleansingAllocator<U> &) noexcept {}

    T *allocate(std::size_t n)
    {
        if (n > std::size_t(-1) / sizeof(T)) {
            throw std::bad_alloc();
        }
        auto *p = static_cast<T *>(::operator new(n * sizeof(T)));
        return p;
    }

    void deallocate(T *p, std::size_t n) noexcept;

    template<typename U> bool operator==(const CleansingAllocator<U> &) const noexcept { return true; }
};

/// Wipe a buffer in a way the optimiser is not allowed to elide.
void secureZero(void *data, std::size_t len) noexcept;

template<typename T> void CleansingAllocator<T>::deallocate(T *p, std::size_t n) noexcept
{
    secureZero(p, n * sizeof(T));
    ::operator delete(p);
}

using SecureVector = std::vector<uint8_t, CleansingAllocator<uint8_t>>;

/**
 * A byte buffer for secret material.
 *
 * Unlike QByteArray this is not implicitly shared, so a copy is always a real
 * copy and destroying an instance really does erase the bytes. Prefer this for
 * anything derived from the master password or the vault keys.
 */
class SecureBytes
{
public:
    SecureBytes() = default;
    SecureBytes(const void *data, qsizetype len);
    explicit SecureBytes(const QByteArray &data);
    explicit SecureBytes(qsizetype len);

    /// Takes ownership of the UTF-8 encoding of @p text.
    static SecureBytes fromString(const QString &text);
    static SecureBytes random(qsizetype len);

    bool isEmpty() const { return m_data.empty(); }
    qsizetype size() const { return qsizetype(m_data.size()); }

    const uint8_t *constData() const { return m_data.data(); }
    uint8_t *data() { return m_data.data(); }

    void resize(qsizetype len) { m_data.resize(size_t(len)); }
    void clear();
    void append(const void *data, qsizetype len);
    void append(const SecureBytes &other);

    SecureBytes mid(qsizetype pos, qsizetype len = -1) const;

    /**
     * Copy into a QByteArray. The result is *not* protected; use only at the
     * boundary where a plaintext value has to be handed to Qt (a QString for the
     * UI, a JSON value, ...).
     */
    QByteArray toByteArray() const;
    QString toString() const;

    /// Constant-time comparison, safe for MAC verification.
    bool operator==(const SecureBytes &other) const;
    bool operator!=(const SecureBytes &other) const { return !(*this == other); }

private:
    SecureVector m_data;
};

/// Constant-time comparison of two buffers of equal length.
bool constantTimeEquals(const QByteArray &a, const QByteArray &b);

} // namespace kvault
