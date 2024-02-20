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
 * Convert filled Keccak instance to a VeXOF
 */
int VeXOF_FromKeccak(VeXOF_Instance *vexof_instance, const Keccak_HashInstance *keccak_instance)
{
    check(!keccak_instance->sponge.squeezing);

    KeccakWidth1600_SpongeInstance spongeInstance;
    uint8_t *states = &vexof_instance->states_data[0];
    uint64_t *d64 = (uint64_t *)states;
    uint64_t *s64 = (uint64_t *)spongeInstance.state;

    memset(vexof_instance, 0, sizeof(VeXOF_Instance));
    vexof_instance->rate = keccak_instance->sponge.rate / 8;
    vexof_instance->byteIOIndex = 0;

    for (uint8_t stripe_idx = 0; stripe_idx < 8; stripe_idx++)
    {
        memcpy(&spongeInstance, &keccak_instance->sponge, sizeof(KeccakWidth1600_SpongeInstance));
        KeccakWidth1600_SpongeAbsorb(&spongeInstance, &stripe_idx, 1);
        KeccakP1600_AddByte(spongeInstance.state, keccak_instance->delimitedSuffix, spongeInstance.byteIOIndex);
        /* If the first bit of padding is at position rate-1, we need a whole new block for the second bit of padding */
        // Consider vectorizing this case
        if ((keccak_instance->delimitedSuffix >= 0x80) && (spongeInstance.byteIOIndex == (spongeInstance.rate / 8 - 1)))
            KeccakP1600_Permute_24rounds(spongeInstance.state);
        KeccakP1600_AddByte(spongeInstance.state, 0x80, spongeInstance.rate / 8 - 1);

        for (int idx = 0; idx < 25; idx++)
        {
#if __AVX512F__
            d64[8 * idx + stripe_idx] = s64[idx];
#else
#if __AVX2__
            d64[4 * idx + stripe_idx + 96 * (stripe_idx / 4)] = s64[idx];
#else
            d64[idx + 25 * stripe_idx] = s64[idx];
#endif
#endif
        }
    }

#if __AVX512F__
    KeccakP1600times8_PermuteAll_24rounds(states);
#else
#if __AVX2__
    KeccakP1600times4_PermuteAll_24rounds(states);
    KeccakP1600times4_PermuteAll_24rounds(states + 800);
#else
    for (unsigned int hi = 0; hi < 8; hi++)
    {
        KeccakP1600_Permute_24rounds(states + 200 * hi);
    }
#endif
#endif

    return 0;
}

/**
 * Squeeze bytes in parallel.
 */
int VeXOF_Squeeze(VeXOF_Instance *vexof_instance, uint8_t *data, size_t dataByteLen)
{
    // Based on KeccakSponge.inc from the XKCP package
    // TODO: Remove 64 byte limitation

    check(dataByteLen % 64 == 0);

    uint8_t *states = &vexof_instance->states_data[0];
    uint32_t rateInBytes = vexof_instance->rate;

    size_t i, j;
    uint32_t partialBlock;
    uint8_t *curData;
    uint32_t stripeRate = rateInBytes * 8;
    uint32_t byteIOIndex = vexof_instance->byteIOIndex;

    i = 0;
    curData = data;
    while (i < dataByteLen)
    {
        if ((byteIOIndex == stripeRate) && (dataByteLen - i >= stripeRate))
        {
            for (j = dataByteLen - i; j >= stripeRate; j -= stripeRate)
            {
#if __AVX512F__
                KeccakP1600times8_PermuteAll_24rounds(states);
                memcpy(curData, states, stripeRate);
#else
#if __AVX2__
                KeccakP1600times4_PermuteAll_24rounds(states);
                KeccakP1600times4_PermuteAll_24rounds(states + 800);
                uint64_t *d64 = (uint64_t *)curData;
                uint64_t *s64 = (uint64_t *)states;
                for (size_t idx2 = 0; idx2 < stripeRate / 64; idx2++)
                    for (uint8_t idx = 0; idx < 4; idx++)
                    {
                        d64[idx + 8 * idx2] = s64[idx + 4 * idx2];
                        d64[4 + idx + 8 * idx2] = s64[100 + idx + 4 * idx2];
                    }
#else
                for (unsigned int hi = 0; hi < 8; hi++)
                {
                    KeccakP1600_Permute_24rounds(states + 200 * hi);
                }
                uint64_t *d64 = (uint64_t *)curData;
                uint64_t *s64 = (uint64_t *)states;
                for (size_t idx2 = 0; idx2 < stripeRate / 64; idx2++)
                    for (uint8_t idx = 0; idx < 8; idx++)
                    {
                        d64[idx + 8 * idx2] = s64[25 * idx + idx2];
                    }
#endif
#endif
                curData += stripeRate;
            }
            i = dataByteLen - j;
        }
        else
        {
            if (byteIOIndex == stripeRate)
            {
#if __AVX512F__
                KeccakP1600times8_PermuteAll_24rounds(states);
#else
#if __AVX2__
                KeccakP1600times4_PermuteAll_24rounds(states);
                KeccakP1600times4_PermuteAll_24rounds(states + 800);
#else
                for (unsigned int hi = 0; hi < 8; hi++)
                {
                    KeccakP1600_Permute_24rounds(states + 200 * hi);
                }
#endif
#endif
                byteIOIndex = 0;
            }
            if (dataByteLen - i > stripeRate - byteIOIndex)
                partialBlock = stripeRate - byteIOIndex;
            else
                partialBlock = (uint32_t)(dataByteLen - i);
            i += partialBlock;
#if __AVX512F__
            memcpy(curData, states + byteIOIndex, partialBlock);
#else
#if __AVX2__
            uint64_t *d64 = (uint64_t *)curData;
            uint64_t *s64 = (uint64_t *)(states + byteIOIndex / 2);
            for (size_t idx2 = 0; idx2 < partialBlock / 64; idx2++)
                for (uint8_t idx = 0; idx < 4; idx++)
                {
                    d64[idx + 8 * idx2] = s64[idx + 4 * idx2];
                    d64[4 + idx + 8 * idx2] = s64[100 + idx + 4 * idx2];
                }
#else
            uint64_t *d64 = (uint64_t *)curData;
            uint64_t *s64 = (uint64_t *)(states + byteIOIndex / 8);
            for (size_t idx2 = 0; idx2 < partialBlock / 64; idx2++)
                for (uint8_t idx = 0; idx < 8; idx++)
                {
                    d64[idx + 8 * idx2] = s64[25 * idx + idx2];
                }
#endif
#endif
            curData += partialBlock;
            byteIOIndex += partialBlock;
        }
    }
    vexof_instance->byteIOIndex = byteIOIndex;
    return 0;
}
