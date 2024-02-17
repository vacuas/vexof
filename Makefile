ARCH=native

# Autodetect AVX
AVX2 := $(findstring  AVX2, $(shell gcc -march=$(ARCH) -dM -E - < /dev/null))
AVX512 := $(findstring  AVX512, $(shell gcc -march=$(ARCH) -dM -E - < /dev/null))

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wredundant-decls -Wshadow -Wvla -Wpointer-arith -O3 -march=$(ARCH) -mtune=$(ARCH) -Wno-unused-variable
SRC = test.c cpucycles.c vexof.c
LIBS = -lcrypto -lm

SRC += FIPS202-timesx/KeccakHash.c FIPS202-timesx/SimpleFIPS202.c FIPS202-timesx/KeccakP-1600-opt64.c FIPS202-timesx/KeccakSponge.c
ifeq ($(AVX512), AVX512)
SRC += FIPS202-timesx/KeccakP-1600-times8-SIMD512.c
else
ifeq ($(AVX2), AVX2)
SRC += FIPS202-timesx/KeccakP-1600-times4-SIMD256.c
endif
endif

all: speed_test

test: $(SRC) Makefile
	$(CC) $(CFLAGS) $(INC) $(SRC) -o test $(LIBS) -DDEBUG

speed_test: test
	./test

clean:
	rm -f ./test
