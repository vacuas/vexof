# VeXOF

Vectorizable SHAKE eXtendable Output Function (XOF).

The VeXOF squeeze is indexed and yields blocks of 168 bytes for SHAKE128. This allows for 4 or 8
parallel instances of SHAKE to operate indepenently. The instances are distinguished by adding a
64 bit block numer before calling the SHAKE Finalize function.

See `reference.c` for a pure SHAKE-only implementation.

## Results

On Skylake `make` outputs:
```
Test ok
Squeeze test ok

XKCP and VeXOF compared to OpenSSL for 40000 bytes (2500 times)
AES:            - 37.759 Kcycles, 0.944 cpb (± 341.0 %)
openssl:        - 355.129 Kcycles, 8.878 cpb (± 10.8 %)
XKCP:           - 343.159 Kcycles, 8.579 cpb (± 9.0 %)
VeXOF:          - 139.730 Kcycles, 3.493 cpb (± 15.4 %)
Reference:      - 368.596 Kcycles, 9.215 cpb (± 10.3 %)
```

On Cascadelake (AVX512) the improvement is much more impressive:
```
Test ok
Squeeze test ok

XKCP and VeXOF compared to OpenSSL for 40000 bytes (2500 times)
AES:            - 23.927 Kcycles, 0.598 cpb (± 361.7 %)
openssl:        - 244.644 Kcycles, 6.116 cpb (± 11.2 %)
XKCP:           - 238.065 Kcycles, 5.952 cpb (± 13.4 %)
VeXOF:          - 39.148 Kcycles, 0.979 cpb (± 10.7 %)
Reference:      - 268.700 Kcycles, 6.718 cpb (± 91.4 %)
```

## API

See `test.c` for examples. The following code snippet illustrates the API:
```
void vexof(const uint8_t *seed, size_t input_bytes, uint8_t *output, int output_bytes)
{
    VeXOF_Instance vexofInstance;
    VeXOF_HashInitialize(&vexofInstance);
    VeXOF_HashUpdate(&vexofInstance, seed, input_bytes);
    VeXOF_Squeeze(&vexofInstance, output, output_bytes);
}
```
where `output_bytes` is the desired output length in bytes. 

`VeXOF_Squeeze` will switch VeXOF from the absorbing to the squeezing state on its first invocation.
`VeXOF_HashUpdate` and `VeXOF_Squeeze` can be called an arbitray number of times.

## Usage

Run 
```
make
```
to run the tests.

## Limitations

In this version the length of the squeezed data must a multiple of 8 bytes.
