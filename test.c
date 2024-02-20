// SPDX-License-Identifier: CC0-1.0
// 2024 Jan Adriaan Leegwater

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <openssl/evp.h>
#include "vexof.h"
int VeXOF_Reference(Keccak_HashInstance *instance_arg, uint8_t *data, size_t dataByteLen);

#define NUM_XOF_BYTES 40000

#define TEST_NUM 2500
// #define REPORT_TIME
#ifdef REPORT_TIME
static inline uint64_t ticks(void)
{
    return (uint64_t)clock();
}

uint64_t ticks_overhead(void)
{
    return 0;
}
#else
#include "cpucycles.h"
#define ticks cpucycles
#define ticks_overhead cpucycles_overhead
#endif

void print_results(const char *s, uint64_t *t, size_t tlenarg)
{
    static uint64_t overhead = -1;

    if (overhead == (uint64_t)-1)
        overhead = ticks_overhead();

    size_t tlen = tlenarg - 1;
    for (size_t i = 0; i < TEST_NUM - 1; ++i)
    {
        t[i] = t[i + 1] - t[i] - overhead;
    }

    uint64_t average = 0;
    for (size_t i = 0; i < TEST_NUM - 1; ++i)
    {
        average += t[i];
    }
    average = average / tlen;

    double variance = 0;
    for (size_t i = 0; i < TEST_NUM - 1; ++i)
    {
        uint64_t diff = (t[i] - average);
        variance += diff * diff;
    }
    double stddef = sqrt(variance / tlen);

#ifdef REPORT_TIME
    printf("%s\t- %.3f ms (± %.1f %%)\n",
           s, average / 1e3, stddef / average * 100);
#else
    printf("%s\t- %.3f Kcycles, %.3f cpb (± %.1f %%)\n",
           s, average / 1e3, average / (float)NUM_XOF_BYTES, stddef / average * 100);
#endif
}

void xkcp(const uint8_t *pt_seed_array, int input_bytes, uint8_t *pt_output_array,
          int output_bytes)
{
    Keccak_HashInstance hashInstance;
    Keccak_HashInitialize_SHAKE128(&hashInstance);
    Keccak_HashUpdate(&hashInstance, pt_seed_array, 8 * input_bytes);
    unsigned char idx = 0;
    Keccak_HashUpdate(&hashInstance, &idx, 8);
    Keccak_HashFinal(&hashInstance, pt_output_array);
    Keccak_HashSqueeze(&hashInstance, pt_output_array, 8 * output_bytes);
}

void vexof_ref(const uint8_t *pt_seed_array, int input_bytes, uint8_t *pt_output_array,
               int output_bytes)
{
    Keccak_HashInstance hashInstance;
    Keccak_HashInitialize_SHAKE128(&hashInstance);
    Keccak_HashUpdate(&hashInstance, pt_seed_array, 8 * input_bytes);
    VeXOF_Reference(&hashInstance, pt_output_array, output_bytes);
}

void vexof(const uint8_t *pt_seed_array, int input_bytes, uint8_t *pt_output_array,
           int output_bytes)
{
    Keccak_HashInstance hashInstance;
    VeXOF_Instance vexofInstance;

    Keccak_HashInitialize_SHAKE128(&hashInstance);
    Keccak_HashUpdate(&hashInstance, pt_seed_array, 8 * input_bytes);

    VeXOF_FromKeccak(&vexofInstance, &hashInstance);
    VeXOF_Squeeze(&vexofInstance, pt_output_array, output_bytes);
}

void shake128(const uint8_t *pt_seed_array, int input_bytes, uint8_t *pt_output_array,
              int output_bytes)
{
    EVP_MD_CTX *context;
    context = EVP_MD_CTX_new();
    EVP_DigestInit_ex(context, EVP_shake128(), NULL);
    EVP_DigestUpdate(context, pt_seed_array, input_bytes);
    EVP_DigestFinalXOF(context, pt_output_array, output_bytes);
    EVP_MD_CTX_free(context);
}

void hash_aes128(const uint8_t *pt_seed_array, uint8_t *pt_output_array)
{
    const uint8_t zero_array[NUM_XOF_BYTES] = {0};
    const uint8_t iv[16] = {0};
    EVP_CIPHER_CTX *context;
    int len;
    context = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(context, EVP_aes_128_ctr(), NULL, pt_seed_array, iv);
    EVP_EncryptUpdate(context, pt_output_array, &len, zero_array,
                      NUM_XOF_BYTES);
    EVP_EncryptFinal_ex(context, pt_output_array + len, &len);
    EVP_CIPHER_CTX_free(context);
}

int main()
{
    uint8_t pt_public_key_seed[16] = {0};
    size_t mlen;
    unsigned char *msg;
    uint8_t digest[64];
    memset(digest, 0, sizeof(digest));

    mlen = 1000;
    msg = (unsigned char *)calloc(mlen, sizeof(unsigned char));
    memset(msg, 0, mlen);

    ALIGN(64)
    uint8_t prng_output_public[NUM_XOF_BYTES] = {0};
    uint8_t prng_output_public_c[NUM_XOF_BYTES] = {0};
    uint64_t test_cycles[TEST_NUM];

    // Test against reference
    vexof(pt_public_key_seed, 16, prng_output_public, NUM_XOF_BYTES);
    vexof_ref(pt_public_key_seed, 16, prng_output_public_c, NUM_XOF_BYTES);

    int testok = 1;
    for (int idx = 0; idx < NUM_XOF_BYTES; idx++)
        if (prng_output_public[idx] != prng_output_public_c[idx])
        {
            printf("Test Failed @ %d: %02x %02x\n", idx, prng_output_public[idx], prng_output_public_c[idx]);
            testok = 0;
            break;
        }
    if (testok)
    {
        printf("Test ok\n");
    }

    // Test multiple squeeze
    {
        Keccak_HashInstance hashInstance;
        VeXOF_Instance vexofInstance;

        Keccak_HashInitialize_SHAKE128(&hashInstance);
        Keccak_HashUpdate(&hashInstance, pt_public_key_seed, 8 * 16);
        VeXOF_FromKeccak(&vexofInstance, &hashInstance);
        VeXOF_Squeeze(&vexofInstance, prng_output_public_c, 2048);
        VeXOF_Squeeze(&vexofInstance, &prng_output_public_c[2048], NUM_XOF_BYTES - 2048);
        testok = 1;
        for (int idx = 0; idx < NUM_XOF_BYTES; idx++)
            if (prng_output_public[idx] != prng_output_public_c[idx])
            {
                printf("Squeeze test Failed @ %d: %02x %02x\n", idx, prng_output_public[idx], prng_output_public_c[idx]);
                testok = 0;
                return 0;
                break;
            }
        if (testok)
        {
            printf("Squeeze test ok\n");
        }
    }

    // Report timings
    printf("\nXKCP and VeXOF compared to OpenSSL for %d bytes (%d times)\n", NUM_XOF_BYTES, TEST_NUM);

    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        pt_public_key_seed[0] = count % 256;
        pt_public_key_seed[1] = count / 256;
        hash_aes128(pt_public_key_seed, prng_output_public);
    }
    print_results("AES:\t", test_cycles, TEST_NUM);

    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        shake128(pt_public_key_seed, 16, prng_output_public, NUM_XOF_BYTES);
    }
    print_results("openssl:", test_cycles, TEST_NUM);

    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        pt_public_key_seed[0] = count % 256;
        pt_public_key_seed[1] = count / 256;
        xkcp(pt_public_key_seed, 16, prng_output_public, NUM_XOF_BYTES);
    }
    print_results("XKCP:\t", test_cycles, TEST_NUM);

    memset(prng_output_public, 0, NUM_XOF_BYTES);
    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        pt_public_key_seed[0] = count % 256;
        pt_public_key_seed[1] = count / 256;
        vexof(pt_public_key_seed, 16, prng_output_public, NUM_XOF_BYTES);
    }
    print_results("VeXOF:\t", test_cycles, TEST_NUM);

    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        pt_public_key_seed[0] = count % 256;
        pt_public_key_seed[1] = count / 256;
        vexof_ref(pt_public_key_seed, 16, prng_output_public_c, NUM_XOF_BYTES);
    }
    print_results("Reference:", test_cycles, TEST_NUM);
}
