# VeXOF

Vectorized SHAKE XOF. Create a XKCP Keccak_HashInstance and squeeze out the bytes in parallel.

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
Keccak_HashInstance hashInstance;
VeXOF_Instance vexofInstance;

Keccak_HashInitialize_SHAKE128(&hashInstance);
Keccak_HashUpdate(&hashInstance, seed, 8 * seed_length);

VeXOF_FromKeccak(&vexofInstance, &hashInstance);
VeXOF_Squeeze(&vexofInstance, data_out, data_out_length);
```

where `data_out_length` is the desired output length in bytes. 

## Usage

Run 
```
make
```
to run the tests. 

## Limitations

In this version the length of the squeezed data (`data_out_length`) must a multiple of 64 bytes.
