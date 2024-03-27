// SPDX-License-Identifier: CC0-1.0
// 2024 Jan Adriaan Leegwater

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <openssl/evp.h>
#include "vexof.h"
int VeXOF_Reference(Keccak_HashInstance *instance_arg, uint8_t *data, size_t dataByteLen);

#define MAX_XOF_BYTES 4000000
#define NUM_XOF_BYTES 32960

#define TEST_NUM 2500

// #define REPORT_TIME
#ifdef REPORT_TIME
static inline uint64_t ticks(void)
{
    return (uint64_t)clock();
}
#else

static inline uint64_t ticks(void)
{
    uint64_t result;

    __asm__ volatile("rdtsc; shlq $32,%%rdx; orq %%rdx,%%rax"
                     : "=a"(result) : : "%rdx");

    return result;
}

#endif

void print_results(const char *s, uint64_t *t, size_t tlenarg, size_t numbytes)
{
    size_t tlen = tlenarg - 1;
    for (size_t i = 0; i < TEST_NUM - 1; ++i)
    {
        t[i] = t[i + 1] - t[i];
    }

    float average = 0;
    for (size_t i = 0; i < TEST_NUM - 1; ++i)
    {
        average += t[i];
    }
    average = average / tlen;

    float variance = 0;
    for (size_t i = 0; i < TEST_NUM - 1; ++i)
    {
        uint64_t diff = (t[i] - average);
        variance += diff * diff;
    }
    float stddef = sqrt(variance / tlen);

#ifdef REPORT_TIME
    printf("%s\t- %.3f µs, %.3f nspb (± %.1f %%)\n",
           s, average, 1.0e3 * average / numbytes, stddef / average * 100);
#else
    printf("%s\t- %.3f Kcycles, %.3f cpb (± %.1f %%)\n",
           s, average / 1e3, average / (float)numbytes, stddef / average * 100);
#endif
}

void xkcp(const uint8_t *pt_seed_array, int input_bytes, uint8_t *pt_output_array,
          int output_bytes)
{
    Keccak_HashInstance hashInstance;
    Keccak_HashInitialize_SHAKE128(&hashInstance);
    Keccak_HashUpdate(&hashInstance, pt_seed_array, 8 * input_bytes);
    Keccak_HashFinal(&hashInstance, pt_output_array);
    Keccak_HashSqueeze(&hashInstance, pt_output_array, 8 * output_bytes);
}

void vexof_ref(const uint8_t *pt_seed_array, int input_bytes, uint64_t *pt_output_array,
               int output_bytes)
{
    Keccak_HashInstance hashInstance;
    Keccak_HashInitialize_SHAKE128(&hashInstance);
    Keccak_HashUpdate(&hashInstance, pt_seed_array, 8 * input_bytes);
    VeXOF_Reference(&hashInstance, (uint8_t *)pt_output_array, output_bytes);
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
    uint8_t pt_public_key_seed[200];
    memset(pt_public_key_seed, 1, 200);
    size_t mlen;
    unsigned char *msg;
    uint8_t digest[64];
    memset(digest, 0, sizeof(digest));

    mlen = 1000;
    msg = (unsigned char *)calloc(mlen, sizeof(unsigned char));
    memset(msg, 0, mlen);

    uint64_t prng_output_public[MAX_XOF_BYTES / 8] = {0};
    uint64_t prng_output_public_c[MAX_XOF_BYTES / 8] = {0};
    uint64_t test_cycles[TEST_NUM];

    // Test against reference
    vexof(pt_public_key_seed, 16, prng_output_public, NUM_XOF_BYTES);
    vexof_ref(pt_public_key_seed, 16, prng_output_public_c, NUM_XOF_BYTES);

    int testok = 1;
    for (int idx = 0; idx < NUM_XOF_BYTES; idx++)
        if (prng_output_public[idx] != prng_output_public_c[idx])
        {
            printf("Test Failed @ %d: %016lx %016lx\n", idx, prng_output_public[idx],
                   prng_output_public_c[idx]);
            testok = 0;
            break;
        }

    if (testok)
    {
        printf("Test ok\n");
    }

    // Test multiple squeeze
    {
        vexof(pt_public_key_seed, 16, prng_output_public, NUM_XOF_BYTES);
        memset(prng_output_public_c, 0, NUM_XOF_BYTES);

        VeXOF_Instance vexofInstance;
        VeXOF_HashInitialize(&vexofInstance);
        VeXOF_HashUpdate(&vexofInstance, pt_public_key_seed, 16);
        VeXOF_Squeeze(&vexofInstance, prng_output_public_c, 2048);
        VeXOF_Squeeze(&vexofInstance, &prng_output_public_c[2048 / 8], 8);
        VeXOF_Squeeze(&vexofInstance, &prng_output_public_c[(2048 + 8) / 8], NUM_XOF_BYTES - 2048 - 8);

        testok = 1;
        for (int idx = 0; idx < NUM_XOF_BYTES / 8; idx++)
            if (prng_output_public[idx] != prng_output_public_c[idx])
            {
                printf("Squeeze test Failed @ %d: %016lx %016lx\n", idx, prng_output_public[idx],
                       prng_output_public_c[idx]);
                testok = 0;
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
        hash_aes128(pt_public_key_seed, (uint8_t *)prng_output_public);
    }
    print_results("AES:\t", test_cycles, TEST_NUM, NUM_XOF_BYTES);

    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        shake128(pt_public_key_seed, 16, (uint8_t *)prng_output_public, NUM_XOF_BYTES);
    }
    print_results("openssl:", test_cycles, TEST_NUM, NUM_XOF_BYTES);

    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        pt_public_key_seed[0] = count % 256;
        pt_public_key_seed[1] = count / 256;
        xkcp(pt_public_key_seed, 16, (uint8_t *)prng_output_public, NUM_XOF_BYTES);
    }
    print_results("XKCP:\t", test_cycles, TEST_NUM, NUM_XOF_BYTES);

    memset(prng_output_public, 0, NUM_XOF_BYTES);
    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        pt_public_key_seed[0] = count % 256;
        pt_public_key_seed[1] = count / 256;
        vexof(pt_public_key_seed, 16, prng_output_public, NUM_XOF_BYTES);
    }
    print_results("VeXOF:\t", test_cycles, TEST_NUM, NUM_XOF_BYTES);

    for (int count = 0; count < TEST_NUM; count++)
    {
        test_cycles[count] = ticks();
        pt_public_key_seed[0] = count % 256;
        pt_public_key_seed[1] = count / 256;
        vexof_ref(pt_public_key_seed, 16, prng_output_public_c, NUM_XOF_BYTES);
    }
    print_results("Reference:", test_cycles, TEST_NUM, NUM_XOF_BYTES);

    // Compare various sizes
    for (int bytes = 64; bytes < 10000; bytes *= 2)
    {
        printf("\nXOF squeeze %d bytes\n", bytes);

        for (int count = 0; count < TEST_NUM; count++)
        {
            test_cycles[count] = ticks();
            pt_public_key_seed[0] = count % 256;
            pt_public_key_seed[1] = count / 256;
            xkcp(pt_public_key_seed, 16, (uint8_t *)prng_output_public, bytes);
        }
        print_results("XKCP\t", test_cycles, TEST_NUM, bytes);

        for (int count = 0; count < TEST_NUM; count++)
        {
            test_cycles[count] = ticks();
            pt_public_key_seed[0] = count % 256;
            pt_public_key_seed[1] = count / 256;
            vexof_ref(pt_public_key_seed, 16, prng_output_public_c, bytes);
        }
        print_results("Reference", test_cycles, TEST_NUM, bytes);

        for (int count = 0; count < TEST_NUM; count++)
        {
            test_cycles[count] = ticks();
            pt_public_key_seed[0] = count % 256;
            pt_public_key_seed[1] = count / 256;
            vexof(pt_public_key_seed, 16, prng_output_public, bytes);
        }
        print_results("VeXOF:\t", test_cycles, TEST_NUM, bytes);
    }
}
