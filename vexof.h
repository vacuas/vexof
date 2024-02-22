// SPDX-License-Identifier: CC0-1.0

/**
 * Vectorized SHAKE XOF based on XKCP.
 *
 * 2024 Jan Adriaan Leegwater
 * https://github.com/vacuas/vexof
 */

#ifndef VEXOF_H
#define VEXOF_H

#include "FIPS202-timesx/KeccakHash.h"

typedef struct
{
    ALIGN(64)
    uint8_t states_data[1600];
    size_t rate;
    size_t byteIOIndex;
    int squeezing;
} VeXOF_Instance;

/**
 * Functions to initialize the VeXOF instance, used in SHAKExxx mode.
 * @param  vexof_instance    Pointer to the hash instance to be initialized.
 * @return KECCAK_SUCCESS if successful, KECCAK_FAIL otherwise.
 */
int VeXOF_HashInitialize_SHAKE128(VeXOF_Instance *vexof_instance);
int VeXOF_HashInitialize_SHAKE256(VeXOF_Instance *vexof_instance);

/**
 * Function to give input data to be absorbed. Can be called multiple times.
 * @param  vexof_instance    Pointer to the VeXOF instance initialized by VeXOF_HashInitialize_SHAKExxx().
 * @param  data              Pointer to the input data.
 * @param  dataByteLen       The number of input bytes provided in the input data.
 * @return KECCAK_SUCCESS if successful, KECCAK_FAIL otherwise.
 */
int VeXOF_HashUpdate(VeXOF_Instance *vexof_instance, const uint8_t *data, size_t dataByteLen);

/**
 * Function to squeeze output data. Can be called multiple times.
 * @param  vexof_instance    Pointer to the VeXOF instance initialized by VeXOF_HashInitialize_SHAKExxx().
 * @param  data              Pointer to the buffer where to store the output data.
 * @param  dataByteLen       The number of output bytes desired (must be a multiple of 32 or 64).
 * @return KECCAK_SUCCESS if successful, KECCAK_FAIL otherwise.
 */
int VeXOF_Squeeze(VeXOF_Instance *vexof_instance, uint64_t *data, size_t dataByteLen);

#endif
