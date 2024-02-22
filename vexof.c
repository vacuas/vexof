// SPDX-License-Identifier: CC0-1.0

/**
 * Vectorized SHAKE XOF based on XKCP.
 *
 * 2024 Jan Adriaan Leegwater
 * https://github.com/vacuas/vexof
 */

#include "vexof.h"

// Options
#ifndef STRIPES
#define STRIPES 8
#endif

/**
 * Warning: Setting USE_AVX512 to 1 will not always improve the performance on AVX512 architectures.
 * Whether is does depends on specifics of the application.
 */
#ifndef USE_AVX512
#define USE_AVX512 __AVX512F__
#endif

#ifndef DEBUG
#define check(x)      \
    {                 \
        if (!(x))     \
            return 1; \
    }
#else
#include <assert.h>
#define check(x) assert(x)
#endif

// Sanity checks
#if KeccakP1600_stateSizeInBytes != 200
#error "KeccakP1600_stateSizeInBytes must be 200"
#endif

#if STRIPES != 4 && STRIPES != 8
#error "STRIPES must be 4 or 8"
#endif

#include <stdlib.h>
#if __AVX2__
#include "FIPS202-timesx/KeccakP-1600-times4-SnP.h"
#if USE_AVX512
#include "FIPS202-timesx/KeccakP-1600-times8-SnP.h"
#endif
#endif

/**
 * Create VeXOF instance
 */
int VeXOF_HashInitialize_SHAKE128(VeXOF_Instance *vexof_instance)
{
    vexof_instance->squeezing = 0;
    return Keccak_HashInitialize_SHAKE128((Keccak_HashInstance *)vexof_instance);
}

int VeXOF_HashInitialize_SHAKE256(VeXOF_Instance *vexof_instance)
{
    vexof_instance->squeezing = 0;
    return Keccak_HashInitialize_SHAKE256((Keccak_HashInstance *)vexof_instance);
}

/**
 * Add bytes to instance
 */
int VeXOF_HashUpdate(VeXOF_Instance *vexof_instance, const uint8_t *data, size_t dataByteLen)
{
    check(!vexof_instance->squeezing);
    return Keccak_HashUpdate((Keccak_HashInstance *)vexof_instance, data, 8 * dataByteLen);
}

/**
 * Convert filled Keccak instance to a VeXOF. Not part of the public API.
 */
int _VeXOF_HashFinal(VeXOF_Instance *vexof_instance)
{
    check(!vexof_instance->squeezing);

    Keccak_HashInstance keccak_instance;
    memcpy(&keccak_instance, (Keccak_HashInstance *)vexof_instance, sizeof(Keccak_HashInstance));

    KeccakWidth1600_SpongeInstance spongeInstance;
    uint8_t *states = &vexof_instance->states_data[0];
    uint64_t *d64 = (uint64_t *)states;
    uint64_t *s64 = (uint64_t *)spongeInstance.state;

    memset(vexof_instance, 0, sizeof(VeXOF_Instance));
    vexof_instance->rate = keccak_instance.sponge.rate / 8;
    vexof_instance->byteIOIndex = 0;

    for (uint8_t stripe_idx = 0; stripe_idx < STRIPES; stripe_idx++)
    {
        memcpy(&spongeInstance, &keccak_instance.sponge, sizeof(KeccakWidth1600_SpongeInstance));
        KeccakWidth1600_SpongeAbsorb(&spongeInstance, &stripe_idx, 1);
        KeccakP1600_AddByte(spongeInstance.state, keccak_instance.delimitedSuffix, spongeInstance.byteIOIndex);
        KeccakP1600_AddByte(spongeInstance.state, 0x80, spongeInstance.rate / 8 - 1);

        for (int idx = 0; idx < 25; idx++)
        {
#if __AVX2__
#if USE_AVX512 || (STRIPES == 4)
            d64[STRIPES * idx + stripe_idx] = s64[idx];
#else
            d64[4 * idx + stripe_idx + 96 * (stripe_idx / 4)] = s64[idx];
#endif
#else
            d64[idx + 25 * stripe_idx] = s64[idx];
#endif
        }
    }

#if __AVX2__
#if USE_AVX512 && STRIPES == 8
    KeccakP1600times8_PermuteAll_24rounds(states);
#else
    KeccakP1600times4_PermuteAll_24rounds(states);
#if STRIPES == 8
    KeccakP1600times4_PermuteAll_24rounds(states + 800);
#endif
#endif
#else
    for (unsigned int hi = 0; hi < STRIPES; hi++)
    {
        KeccakP1600_Permute_24rounds(states + 200 * hi);
    }
#endif
    vexof_instance->squeezing = 1;

    return 0;
}

/**
 * Squeeze bytes in parallel.
 */
int VeXOF_Squeeze(VeXOF_Instance *vexof_instance, uint64_t *data, size_t dataByteLen)
{
    // Based on KeccakSponge.inc from the XKCP package
    // TODO: Remove (STRIPES * 8) bytes limitation

    check(dataByteLen % (STRIPES * 8) == 0);

    if (!vexof_instance->squeezing)
    {
        int res = _VeXOF_HashFinal(vexof_instance);
        if (res)
            return res;
    }

    uint8_t *states = &vexof_instance->states_data[0];
    uint32_t rateInBytes = vexof_instance->rate;

    size_t i, j;
    uint32_t partialBlock;
    uint8_t *curData;
    uint32_t stripeRate = rateInBytes * STRIPES;
    uint32_t byteIOIndex = vexof_instance->byteIOIndex;

    i = 0;
    curData = (uint8_t *)data;
    while (i < dataByteLen)
    {
        if ((byteIOIndex == stripeRate) && (dataByteLen - i >= stripeRate))
        {
            for (j = dataByteLen - i; j >= stripeRate; j -= stripeRate)
            {
#if __AVX2__
#if USE_AVX512 && STRIPES == 8
                KeccakP1600times8_PermuteAll_24rounds(states);
                memcpy(curData, states, stripeRate);
#else
                KeccakP1600times4_PermuteAll_24rounds(states);
#if STRIPES == 8
                KeccakP1600times4_PermuteAll_24rounds(states + 800);
                uint64_t *d64 = (uint64_t *)curData;
                uint64_t *s64 = (uint64_t *)states;
                for (size_t idx2 = 0; idx2 < stripeRate / (8 * STRIPES); idx2++)
                    for (uint8_t idx = 0; idx < 4; idx++)
                    {
                        d64[idx + STRIPES * idx2] = s64[idx + 4 * idx2];
                        d64[4 + idx + STRIPES * idx2] = s64[100 + idx + 4 * idx2];
                    }
#else
                memcpy(curData, states, stripeRate);
#endif
#endif
#else
                for (unsigned int hi = 0; hi < STRIPES; hi++)
                {
                    KeccakP1600_Permute_24rounds(states + 200 * hi);
                }
                uint64_t *d64 = (uint64_t *)curData;
                uint64_t *s64 = (uint64_t *)states;
                for (size_t idx2 = 0; idx2 < stripeRate / (8 * STRIPES); idx2++)
                    for (uint8_t idx = 0; idx < 8; idx++)
                    {
                        d64[idx + STRIPES * idx2] = s64[25 * idx + idx2];
                    }
#endif
                curData += stripeRate;
            }
            i = dataByteLen - j;
        }
        else
        {
            if (byteIOIndex == stripeRate)
            {
#if __AVX2__
#if USE_AVX512 && STRIPES == 8
                KeccakP1600times8_PermuteAll_24rounds(states);
#else
                KeccakP1600times4_PermuteAll_24rounds(states);
#if STRIPES == 8
                KeccakP1600times4_PermuteAll_24rounds(states + 800);
#endif
#endif
#else
                for (unsigned int hi = 0; hi < STRIPES; hi++)
                {
                    KeccakP1600_Permute_24rounds(states + 200 * hi);
                }
#endif
                byteIOIndex = 0;
            }
            if (dataByteLen - i > stripeRate - byteIOIndex)
                partialBlock = stripeRate - byteIOIndex;
            else
                partialBlock = (uint32_t)(dataByteLen - i);
            i += partialBlock;
#if __AVX2__
#if STRIPES == 4 || USE_AVX512
            memcpy(curData, states + byteIOIndex, partialBlock);
#else
            uint64_t *d64 = (uint64_t *)curData;
            uint64_t *s64 = (uint64_t *)(states + byteIOIndex / (STRIPES / 4));
            for (size_t idx2 = 0; idx2 < partialBlock / (8 * STRIPES); idx2++)
                for (uint8_t idx = 0; idx < 4; idx++)
                {
                    d64[idx + STRIPES * idx2] = s64[idx + 4 * idx2];
                    d64[4 + idx + STRIPES * idx2] = s64[100 + idx + 4 * idx2];
                }
#endif
#else
            uint64_t *d64 = (uint64_t *)curData;
            uint64_t *s64 = (uint64_t *)(states + byteIOIndex / STRIPES);
            for (size_t idx2 = 0; idx2 < partialBlock / (8 * STRIPES); idx2++)
                for (uint8_t idx = 0; idx < STRIPES; idx++)
                {
                    d64[idx + STRIPES * idx2] = s64[25 * idx + idx2];
                }
#endif
            curData += partialBlock;
            byteIOIndex += partialBlock;
        }
    }
    vexof_instance->byteIOIndex = byteIOIndex;
    return 0;
}
