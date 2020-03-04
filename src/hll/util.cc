#include "util.h"

uint64_t utility::hashInt(int i)
{
    // Structure that is 160 bits wide used to extract 64 bits from a SHA-1.
    struct hashval
    {
        uint64_t high64;
        char low96[12];
    } hash;

    // Calculate the SHA-1 hash of the integer.
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, (unsigned char *)&i, sizeof(i));
    SHA1_Final((unsigned char *)&hash, &ctx);

    // Return 64 bits of the hash.
    return hash.high64;
}