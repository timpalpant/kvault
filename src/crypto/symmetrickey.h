#pragma once

#include "securebytes.h"

namespace kvault {

/**
 * A Bitwarden symmetric key.
 *
 * Keys are either 32 bytes (encryption only, legacy type 0 EncStrings) or
 * 64 bytes, in which case the first half is the AES key and the second half is
 * the HMAC key.
 */
class SymmetricKey
{
public:
    SymmetricKey() = default;
    explicit SymmetricKey(SecureBytes keyMaterial);

    /// A fresh 512-bit key (256-bit AES + 256-bit MAC), as used for new ciphers.
    static SymmetricKey generate();

    /**
     * Expand a 32-byte key into an encryption/MAC pair using HKDF-Expand.
     * This is how the master key becomes the "stretched master key" that
     * protects the user key.
     */
    static SymmetricKey stretch(const SecureBytes &key32);

    bool isValid() const { return m_encKey.size() == 32 || m_encKey.size() == 16; }
    bool hasMacKey() const { return !m_macKey.isEmpty(); }

    const SecureBytes &encKey() const { return m_encKey; }
    const SecureBytes &macKey() const { return m_macKey; }
    /// The full key material, i.e. what gets wrapped when this key is stored.
    const SecureBytes &full() const { return m_full; }

    void clear();

private:
    SecureBytes m_full;
    SecureBytes m_encKey;
    SecureBytes m_macKey;
};

} // namespace kvault
