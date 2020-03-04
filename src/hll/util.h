#ifndef UTIL_H
#define UTIL_H

#include <inttypes.h>
#include <openssl/sha.h>

using namespace std;

namespace utility
{
uint64_t hashInt(int i);
}

#endif