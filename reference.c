// SPDX-License-Identifier: CC0-1.0

/**
 * Vectorized SHAKE XOF based on XKCP.
 *
 * 2024 Jan Adriaan Leegwater
 */

#include "FIPS202-timesx/KeccakHash.h"

#define STRIPES 8

#include <stdlib.h>

#ifdef DEBUG
#include <assert.h>
#define check(x) assert(x)
#else
#define check(x)      \
    {                 \
        if (!(x))     \
            return 1; \
    }
#endif

/**
 * Reference version for testing compliance to the specification.
 */
int VeXOF_Reference(Keccak_HashInstance *instance_arg, uint8_t *data, size_t dataByteLen)
{
    Keccak_HashInstance hashInstance;
    uint64_t *b64;
    uint64_t *d64;
    size_t num_bytes = dataByteLen / STRIPES;

    check(!instance_arg->sponge.squeezing);
    check(dataByteLen % (8 * STRIPES) == 0);
    check(instance_arg->fixedOutputLength == 0);

    d64 = (uint64_t *)data;
    b64 = malloc(num_bytes);

    for (uint8_t idx = 0; idx < STRIPES; idx++)
    {
        memcpy(&hashInstance, instance_arg, sizeof(Keccak_HashInstance));
        Keccak_HashUpdate(&hashInstance, &idx, 8);
        Keccak_HashFinal(&hashInstance, NULL);
        Keccak_HashSqueeze(&hashInstance, (uint8_t *)b64, 8 * num_bytes);
        for (size_t idx2 = 0; idx2 < num_bytes / 8; idx2++)
            d64[idx + STRIPES * idx2] = b64[idx2];
    }
    free(b64);

    return 0;
}
