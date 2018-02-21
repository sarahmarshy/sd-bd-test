#include "mbed.h"

#include "SDBlockDevice.h"
#include <stdlib.h>


#define TEST_BLOCK_COUNT        10
#define TEST_ERROR_MASK         16
#define TEST_BLOCK_SIZE         2048

const struct {
    const char *name;
    bd_size_t (BlockDevice::*method)() const;
} ATTRS[] = {
    {"read size",    &BlockDevice::get_read_size},
    {"program size", &BlockDevice::get_program_size},
    {"erase size",   &BlockDevice::get_erase_size},
    {"total size",   &BlockDevice::size},
};

int  main() {
    SDBlockDevice sd(D11, D12, D13, D10);

    int err = sd.init();
    MBED_ASSERT(err == 0);

    err = sd.frequency(8000000);
    MBED_ASSERT(err == 0);

    for (unsigned a = 0; a < sizeof(ATTRS)/sizeof(ATTRS[0]); a++) {
        static const char *prefixes[] = {"", "k", "M", "G"};
        for (int i = 3; i >= 0; i--) {
            bd_size_t size = (sd.*ATTRS[a].method)();
            if (size >= (1ULL << 10*i)) {
                printf("%s: %llu%sbytes (%llubytes)\n",
                    ATTRS[a].name, size >> 10*i, prefixes[i], size);
                break;
            }
        }
    }

    bd_size_t erase_size = sd.get_erase_size();
    bd_size_t block_size = erase_size > TEST_BLOCK_SIZE ? erase_size : TEST_BLOCK_SIZE;

    uint8_t *write_block = new uint8_t[block_size];
    uint8_t *read_block = new uint8_t[block_size];
    uint8_t *error_mask = new uint8_t[TEST_ERROR_MASK];
    unsigned addrwidth = ceil(log(float(sd.size()-1)) / log(float(16)))+1;

    for (int b = 0; b < TEST_BLOCK_COUNT; b++) {
        // Find a random block
        bd_addr_t block = (rand()*block_size) % sd.size();

        // Use next random number as temporary seed to keep
        // the address progressing in the pseudorandom sequence
        unsigned seed = rand();

        // Fill with random sequence
        srand(seed);
        for (bd_size_t i = 0; i < block_size; i++) {
            write_block[i] = 0xff & rand();
        }

        // Write, sync, and read the block
        printf("test  %0*llx:%llu...\n", addrwidth, block, block_size);

        err = sd.trim(block, block_size);
        MBED_ASSERT(err == 0);

        err = sd.program(write_block, block, block_size);
        MBED_ASSERT(err == 0);

        printf("write %0*llx:%llu ", addrwidth, block, block_size);
        for (int i = 0; i < 16; i++) {
            printf("%02x", write_block[i]);
        }
        printf("...\n");

        err = sd.read(read_block, block, block_size);
        MBED_ASSERT(err == 0);

        printf("read  %0*llx:%llu ", addrwidth, block, block_size);
        for (int i = 0; i < 16; i++) {
            printf("%02x", read_block[i]);
        }
        printf("...\n");

        // Find error mask for debugging
        memset(error_mask, 0, TEST_ERROR_MASK);
        bd_size_t error_scale = block_size / (TEST_ERROR_MASK*8);

        srand(seed);
        for (bd_size_t i = 0; i < TEST_ERROR_MASK*8; i++) {
            for (bd_size_t j = 0; j < error_scale; j++) {
                if ((0xff & rand()) != read_block[i*error_scale + j]) {
                    error_mask[i/8] |= 1 << (i%8);
                }
            }
        }

        printf("error %0*llx:%llu ", addrwidth, block, block_size);
        for (int i = 0; i < 16; i++) {
            printf("%02x", error_mask[i]);
        }
        printf("\n");

        // Check that the data was unmodified
        srand(seed);
        for (bd_size_t i = 0; i < block_size; i++) {
           MBED_ASSERT(0xff & rand() == read_block[i]);
        }
    }

    err = sd.deinit();
    MBED_ASSERT(err == 0);
}

