#include "symmetrickey.h"

#include "crypto.h"

namespace kvault {

SymmetricKey::SymmetricKey(SecureBytes keyMaterial)
    : m_full(std::move(keyMaterial))
{
    switch (m_full.size()) {
    case 32:
        m_encKey = m_full.mid(0, 32);
        break;
    case 64:
        m_encKey = m_full.mid(0, 32);
        m_macKey = m_full.mid(32, 32);
        break;
    case 16:
        // AES-128 without MAC; only seen in very old vaults.
        m_encKey = m_full.mid(0, 16);
        break;
    default:
        m_full.clear();
        break;
    }
}

SymmetricKey SymmetricKey::generate()
{
    return SymmetricKey(SecureBytes::random(64));
}

SymmetricKey SymmetricKey::stretch(const SecureBytes &key32)
{
    if (key32.size() != 32) {
        return {};
    }
    SecureBytes material = Crypto::hkdfExpandSha256(key32, QByteArrayLiteral("enc"), 32);
    material.append(Crypto::hkdfExpandSha256(key32, QByteArrayLiteral("mac"), 32));
    return SymmetricKey(std::move(material));
}

void SymmetricKey::clear()
{
    m_full.clear();
    m_encKey.clear();
    m_macKey.clear();
}

} // namespace kvault
