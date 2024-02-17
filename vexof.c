// SPDX-License-Identifier: CC0-1.0

/**
 * Vectorized SHAKE XOF based on XKCP.
 *
 * 2024 Jan Adriaan Leegwater
 */

#include "vexof.h"

#if KeccakP1600_stateSizeInBytes != 200
#error "KeccakP1600_stateSizeInBytes must be 200"
#endif

#if __AVX512F__
#include "FIPS202-timesx/KeccakP-1600-times8-SnP.h"
#else
#if __AVX2__
#include "FIPS202-timesx/KeccakP-1600-times4-SnP.h"
#endif
#endif

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
 * Reference version as fallback when AVX2 is not available.
 * Can also be used for testing compliance to the specification.
 */
int VeXOF_Reference(Keccak_HashInstance *instance_arg, uint8_t *data, size_t dataByteLen)
{
    Keccak_HashInstance hashInstance;
    uint64_t *b64;
    uint64_t *d64;
    size_t num_bytes = dataByteLen / 8;

    check(!instance_arg->sponge.squeezing);
    check(dataByteLen % 64 == 0);

    d64 = (uint64_t *)data;
    b64 = malloc(dataByteLen / 8);

    for (uint8_t idx = 0; idx < 8; idx++)
    {
        memcpy(&hashInstance, instance_arg, sizeof(Keccak_HashInstance));
        Keccak_HashUpdate(&hashInstance, &idx, 8);
        // Reset desired output bytes in case it was set
        hashInstance.fixedOutputLength = 0;
        Keccak_HashFinal(&hashInstance, NULL);
        Keccak_HashSqueeze(&hashInstance, (uint8_t *)b64, 8 * num_bytes);
        for (size_t idx2 = 0; idx2 < num_bytes / 8; idx2++)
            d64[idx + 8 * idx2] = b64[idx2];
    }
    free(b64);

    return 0;
}

/**
 * Convert filled Keccak instance to a VeXOF
 */
int VeXOF_Initialize(VeXOF_Instance *vexof_instance, Keccak_HashInstance *keccak_instance)
{
    check(!keccak_instance->sponge.squeezing);

#if __AVX2__
    KeccakWidth1600_SpongeInstance spongeInstance;
    uint8_t *states = &vexof_instance->states_data[0];
    uint64_t *d64 = (uint64_t *)states;
    uint64_t *s64 = (uint64_t *)spongeInstance.state;

    memset(vexof_instance, 0, sizeof(VeXOF_Instance));
    vexof_instance->rate = keccak_instance->sponge.rate / 8;
    vexof_instance->byteIOIndex = 0;

    for (uint8_t lane_idx = 0; lane_idx < 8; lane_idx++)
    {
        memcpy(&spongeInstance, &keccak_instance->sponge, sizeof(KeccakWidth1600_SpongeInstance));
        KeccakWidth1600_SpongeAbsorb(&spongeInstance, &lane_idx, 1);
        KeccakP1600_AddByte(spongeInstance.state, keccak_instance->delimitedSuffix, spongeInstance.byteIOIndex);
        KeccakP1600_AddByte(spongeInstance.state, 0x80, spongeInstance.rate / 8 - 1);

        for (int idx = 0; idx < 25; idx++)
        {
#if __AVX512F__
            d64[8 * idx + lane_idx] = s64[idx];
#else
            d64[4 * idx + lane_idx + 96 * (lane_idx / 4)] = s64[idx];
#endif
        }
    }

#if __AVX512F__
    KeccakP1600times8_PermuteAll_24rounds(states);
#else
    KeccakP1600times4_PermuteAll_24rounds(states);
    KeccakP1600times4_PermuteAll_24rounds(states + 800);
#endif

#else
    // No AVX2
    memcpy(vexof_instance, keccak_instance, sizeof(Keccak_HashInstance));
#endif

    return 0;
}

/**
 * Squeeze bytes in parallel.
 */
int VeXOF_Squeeze(VeXOF_Instance *vexof_instance, uint8_t *data, size_t dataByteLen)
{
    // Based on KeccakSponge.inc from the XKCP package

    check(dataByteLen % 64 == 0);

#if __AVX2__
    uint8_t *states = &vexof_instance->states_data[0];
    uint32_t rateInBytes = vexof_instance->rate;

    size_t i, j;
    uint32_t partialBlock;
    uint8_t *curData;
    uint32_t laneRate = rateInBytes * 8;
    uint32_t byteIOIndex = vexof_instance->byteIOIndex;

    i = 0;
    curData = data;
    while (i < dataByteLen)
    {
        if ((byteIOIndex == laneRate) && (dataByteLen - i >= laneRate))
        {
            for (j = dataByteLen - i; j >= laneRate; j -= laneRate)
            {
#if __AVX512F__
                KeccakP1600times8_PermuteAll_24rounds(states);
                memcpy(curData, states, laneRate);
#else
                KeccakP1600times4_PermuteAll_24rounds(states);
                KeccakP1600times4_PermuteAll_24rounds(states + 800);
                uint64_t *d64 = (uint64_t *)curData;
                uint64_t *s64 = (uint64_t *)states;
                for (size_t idx2 = 0; idx2 < laneRate / 64; idx2++)
                    for (uint8_t idx = 0; idx < 4; idx++)
                    {
                        d64[idx + 8 * idx2] = s64[idx + 4 * idx2];
                        d64[4 + idx + 8 * idx2] = s64[100 + idx + 4 * idx2];
                    }
#endif
                curData += laneRate;
            }
            i = dataByteLen - j;
        }
        else
        {
            if (byteIOIndex == laneRate)
            {
#if __AVX512F__
                KeccakP1600times8_PermuteAll_24rounds(states);
#else
                KeccakP1600times4_PermuteAll_24rounds(states);
                KeccakP1600times4_PermuteAll_24rounds(states + 800);
#endif
                byteIOIndex = 0;
            }
            if (dataByteLen - i > laneRate - byteIOIndex)
                partialBlock = laneRate - byteIOIndex;
            else
                partialBlock = (uint32_t)(dataByteLen - i);
            i += partialBlock;
#if __AVX512F__
            memcpy(curData, states + byteIOIndex, partialBlock);
#else
            uint64_t *d64 = (uint64_t *)curData;
            uint64_t *s64 = (uint64_t *)(states + byteIOIndex / 2);
            for (size_t idx2 = 0; idx2 < partialBlock / 64; idx2++)
                for (uint8_t idx = 0; idx < 4; idx++)
                {
                    d64[idx + 8 * idx2] = s64[idx + 4 * idx2];
                    d64[4 + idx + 8 * idx2] = s64[100 + idx + 4 * idx2];
                }
#endif
            curData += partialBlock;
            byteIOIndex += partialBlock;
        }
    }
    vexof_instance->byteIOIndex = byteIOIndex;
    return 0;

#else
    // No AVX
    return VeXOF_Reference((Keccak_HashInstance *)vexof_instance, data, dataByteLen);
#endif
}
