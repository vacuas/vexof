// SPDX-License-Identifier: CC0-1.0

/**
 * Vectorized SHAKE XOF
 *
 * 2024 Jan Adriaan Leegwater
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
} VeXOF_Instance;

int VeXOF_FromKeccak(VeXOF_Instance *vexof_instance, const Keccak_HashInstance *keccak_instance);
int VeXOF_Squeeze(VeXOF_Instance *vexof_instance, uint8_t *data, size_t dataByteLen);

#endif
