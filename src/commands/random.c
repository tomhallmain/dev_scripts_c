#include "../include/dsc.h"
#include "posix_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#endif

#define MODE_NUM 0
#define MODE_TEXT 1
#define MODE_ERR 2

void seed_random() {
    unsigned int seed;
    
    #ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptGenRandom(hCryptProv, sizeof(seed), (BYTE*)&seed)) {
            CryptReleaseContext(hCryptProv, 0);
            srand(seed);
            return;
        }
        CryptReleaseContext(hCryptProv, 0);
    }
    // Fallback to GetTickCount if crypto API fails
    seed = (unsigned int)GetTickCount();
    #else
    // Try /dev/urandom first
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        if (fread(&seed, sizeof(seed), 1, urandom) == 1) {
            fclose(urandom);
            srand(seed);
            return;
        }
        fclose(urandom);
    }
    // Fallback to time if urandom fails
    seed = (unsigned int)time(NULL);
    #endif
    
    srand(seed);
}

double get_random_double_one_to_zero() {
    double rand1 = (double)rand();
    double rand2 = (double)rand();

    if (rand1 > rand2) {
        return rand2 / rand1;
    } else {
        return rand1 / rand2;
    }
}

// Get a random number or do soft text anonymization for generating random text
// data.
int get_random(int argc, char **argv, data_file_t *file) {
    int mode = MODE_ERR;
    DEBUG_PRINT("random - Running random");

    if (argc == 0) {
        mode = file->is_piped ? MODE_TEXT : MODE_NUM;
    } else {
        char *mode_str = argv[0];
        if (!mode_str || strcmp(mode_str, "") == 0) {
            mode = file->is_piped ? MODE_TEXT : MODE_NUM;
        } else {
            size_t size = strlen(mode_str);
            if (size > 6) {
                FAIL("random - [mode] not understood - available options: "
                    "[number|text]");
            }

            char mode_pattern[8];
            stpcpy(mode_pattern, "^");
            stpcpy(mode_pattern, mode_str);

            if (rematch(mode_pattern, "number", false)) {
              mode = MODE_NUM;
            } else if (rematch(mode_pattern, "text", false)) {
              mode = MODE_TEXT;
            }
        }
    }

    if (mode == MODE_NUM) {
        seed_random();
        printf("%f", get_random_double_one_to_zero());
        return 0;
    } else if (mode == MODE_ERR) {
        FAIL("random - [mode] not understood - available options: [number|text]");
    }

    FILE *fp = get_readable_fp(file);
    int chars_seen = 0;
    int c;

    while ((c = fgetc(fp)) != EOF) {
        if (chars_seen++ % 150 == 0) {
            seed_random();
        }
        if (c < 48 || c > 122) {
            printf("%c", (char)c);
        } else if ((c > 58 && c < 65) || (c > 90 && c < 97)) {
            printf("%c", (char)c);
        } else if (c > 96) {
            printf("%c", ((int)(get_random_double_one_to_zero() * (121 - 97))) + 97);
        } else if (c < 58) {
            printf("%c", ((int)(get_random_double_one_to_zero() * (57 - 48))) + 48);
        } else {
            printf("%c", ((int)(get_random_double_one_to_zero() * (90 - 65))) + 65);
        }
    }

    return 0;
}
