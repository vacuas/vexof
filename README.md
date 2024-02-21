# VeXOF

Vectorizable SHAKE eXtendable Output Function (XOF).

In the squeezing phase, VeXOF creates 4 or 8 instances (called stripes) of SHAKE that operate indepenently.
The instances are distinguished by adding a single byte with the stripe number when transitioning
from absorbing to squeezing state. The XOF output is composed of 8 bytes from stripe 0, then 8 bytes 
from stripe 1 and so on up to 64 bytes before stripe 0 is used again. In this way, 8 parallel instances
of SHAKE can run vectorized resulting in a significant speedup of the XOF.

See `reference.c` for a pure SHAKE-only implementation.

## Results

On Skylake `make` outputs:
```
Test ok
Squeeze test ok

XKCP and VeXOF compared to OpenSSL for 40000 bytes (2500 times)
AES:            - 36.886 Kcycles, 0.922 cpb (± 297.4 %)
openssl:        - 355.473 Kcycles, 8.887 cpb (± 11.7 %)
XKCP:           - 342.078 Kcycles, 8.552 cpb (± 9.2 %)
VeXOF:          - 134.597 Kcycles, 3.365 cpb (± 18.7 %)
Reference:      - 375.682 Kcycles, 9.392 cpb (± 33.4 %)
```

On Cascadelake (AVX512) the improvement is much more impressive:
```
Test ok
Squeeze test ok

XKCP and VeXOF compared to OpenSSL for 40000 bytes (2500 times)
AES:            - 24.592 Kcycles, 0.615 cpb (± 434.2 %)
openssl:        - 256.793 Kcycles, 6.420 cpb (± 18.7 %)
XKCP:           - 238.101 Kcycles, 5.953 cpb (± 10.6 %)
VeXOF:          - 32.133 Kcycles, 0.803 cpb (± 19.4 %)
Reference:      - 259.059 Kcycles, 6.476 cpb (± 14.9 %)
```

## API

See `test.c` for examples. The following code snippet illustrates the API:

```
VeXOF_Instance vexofInstance;
VeXOF_HashInitialize_SHAKE128(&vexofInstance);
VeXOF_HashUpdate(&vexofInstance, seed, seed_length);
VeXOF_Squeeze(&vexofInstance, data_out, data_out_length);
```

where `data_out_length` is the desired output length in bytes. 

`VeXOF_HashUpdate` and `VeXOF_Squeeze` can be called an arbitray number of times.
On its first invocation `VeXOF_Squeeze` will switch to the sequeezing state.

## Usage

Run 
```
make
```
to run the tests.

## Limitations

In this version the length of the squeezed data (`data_out_length`) must a multiple 
of 32 or 64 bytes depending on the number of stripes.
