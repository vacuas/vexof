# VeXOF

Vectorized SHAKE XOF. Use XKCP Keccak_HashInstance and squeeze out the bytes in parallel.

## Usage

Run 
```
make
```
to run the tests. 

## API

See test.c for examples. The following code snippet illustrates the API

```
Keccak_HashInstance hashInstance;
VeXOF_Instance vexofInstance;

Keccak_HashInitialize_SHAKE128(&hashInstance);
Keccak_HashUpdate(&hashInstance, seed, 8 * seed_length);

VeXOF_Initialize(&vexofInstance, &hashInstance);
VeXOF_Squeeze(&vexofInstance, data_out, data_length);
```

Here data_length is in bytes. 

## Limitations

In this version the length of the squeezed data (data_length) must a multiple of 64 bytes.
