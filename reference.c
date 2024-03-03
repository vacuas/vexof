// SPDX-License-Identifier: CC0-1.0

/**
 * Vectorized SHAKE XOF based on XKCP.
 *
 * 2024 Jan Adriaan Leegwater
 */

#include "FIPS202-timesx/KeccakHash.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/**
 * Reference version for testing compliance to the specification.
 */
int VeXOF_Reference(Keccak_HashInstance *instance_arg, uint8_t *data, size_t num_bytes)
{
    Keccak_HashInstance hashInstance;

    assert(num_bytes % 64 == 0);
    assert(!instance_arg->sponge.squeezing);
    assert(instance_arg->fixedOutputLength == 0);

    size_t index = 0;
    uint64_t block = 0;

    while (index < num_bytes)
    {
        memcpy(&hashInstance, instance_arg, sizeof(Keccak_HashInstance));
        Keccak_HashUpdate(&hashInstance, (uint8_t *)&block, 64);
        Keccak_HashFinal(&hashInstance, NULL);
        block++;

        size_t bytes = num_bytes - index;
        if (bytes > (instance_arg->sponge.rate / 8))
            bytes = (instance_arg->sponge.rate / 8);

        Keccak_HashSqueeze(&hashInstance, data, 8 * bytes);

        data += bytes;
        index += bytes;
    }

    return 0;
}
