#pragma once

#include <QStringList>

namespace kvault {

/**
 * The EFF "large" diceware wordlist (7776 words), the same list Bitwarden uses
 * for passphrase generation.
 */
namespace Wordlist {

int size();
QString word(int index);

} // namespace Wordlist

} // namespace kvault
