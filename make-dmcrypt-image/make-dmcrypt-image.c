/* ___    ___
 * \  \  /  /
 *  \  \/  /   Copyright (c) Fixposition AG
 *  /  /\  \
 * /__/  \__\
 *
 * Tool to create dm-crypt compatible disk images
 *
 * MIT License
 *
 * Copyright (c) Fixposition AG (www.fixposition.com) and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <kcapi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNUSED(thing) (void)thing
#define NUMOF(x) (int)(sizeof(x) / sizeof(*(x)))
#define SECTOR_SIZE 512

typedef enum scheme_e {
    SCHEME_UNSPECIFIED = 0,
    SCHEME_AES_XTS_PLAIN = 1,
    SCHEME_AES_CBC_ESSIV_SHA256 = 2,
} scheme_t;

static const char* CipherName(const scheme_t scheme) {
    switch (scheme) {  // clang-format off
        case SCHEME_UNSPECIFIED:          break;
        case SCHEME_AES_XTS_PLAIN:        return "xts(aes)";
        case SCHEME_AES_CBC_ESSIV_SHA256: return "cbc(aes)";
    }  // clang-format on
    return "?";
}

static const char* SchemeName(const scheme_t scheme) {
    switch (scheme) {  // clang-format off
        case SCHEME_UNSPECIFIED:          break;
        case SCHEME_AES_XTS_PLAIN:        return "aes-xts-plain";
        case SCHEME_AES_CBC_ESSIV_SHA256: return "aes-cbc-essiv:sha256";
    }  // clang-format on
    return "?";
}

int main(int argc, char** argv) {
    const char* in_file = NULL;
    const char* out_file = NULL;
    const char* key_file = NULL;
    scheme_t scheme = SCHEME_UNSPECIFIED;
    int verbosity = 0;
    bool ok = true;
    {
        static const struct option options[] =  // clang-format off
        {
            { "in",      required_argument, NULL, 'i' },
            { "out",     required_argument, NULL, 'o' },
            { "key",     required_argument, NULL, 'k' },
            { "scheme",  required_argument, NULL, 's' },
            { "help",    no_argument,       NULL, 'h' },
            { NULL,      0,                 NULL, 0   },
        };  // clang-format on
        int optIx = 0;
        while (ok) {
            const int opt = getopt_long(argc, argv, "i:o:k:s:hv", options, &optIx);
            if (opt < 0) {
                break;
            }
            switch (opt) {
                case 'h':
                    printf(
                        "Tool to create dm-crypt compatible disk images\n"
                        "Copyright (c) Fixposition AG\n"
                        "\n"
                        "Usage:\n"
                        "\n"
                        "    %s [-v] -s <scheme> -i <inputfile> -o <outputfile> -k <keyfile>\n"
                        "\n"
                        "Where:\n"
                        "\n"
                        "   -i <inputfile> is the input (unecncrypted) disk image\n"
                        "   -o <outputfile> is the output (ecncrypted) disk image\n"
                        "   -k <keyfile> is key file (size depends on scheme)\n"
                        "   -s <scheme> selects the encryption scheme:\n"
                        "      1 = aes-xts-plain              key size: 32 or 64 bytes (256 or 512 bits)\n"
                        "      2 = aes-cbc-essiv:sha256       key size: 16 bytes (128 bits)\n"
                        "   -v increases verbosity (multiple -v can be given)\n"
                        "\n",
                        argv[0]);
                    exit(EXIT_SUCCESS);
                case 'v':
                    verbosity++;
                    break;
                case 'i':
                    in_file = optarg;
                    break;
                case 'o':
                    out_file = optarg;
                    break;
                case 'k':
                    key_file = optarg;
                    break;
                case 's':
                    switch (atoi(optarg)) {
                        case SCHEME_AES_XTS_PLAIN:
                            scheme = SCHEME_AES_XTS_PLAIN;
                            break;
                        case SCHEME_AES_CBC_ESSIV_SHA256:
                            scheme = SCHEME_AES_CBC_ESSIV_SHA256;
                            break;
                    }
                    break;
            }
        }
        if ((in_file == NULL) || (out_file == NULL) || (key_file == NULL) || (scheme == SCHEME_UNSPECIFIED)) {
            ok = false;
        }
    }

    fprintf(stderr, "make-dmcrypt-image -- copyright (c) Fixposition AG\n");
    if (!ok) {
        fprintf(stderr, "Bad or missing arguments. Try '%s -h'.\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Debugging for libkcapi
    if (verbosity > 0) {
        kcapi_set_verbosity(KCAPI_LOG_DEBUG);
    }

    // Create Kernel Crypto API handles
    const char* cipher = CipherName(scheme);
    // - For encryption of the sector
    struct kcapi_handle* kch_enc = NULL;
    {
        const int res = kcapi_cipher_init(&kch_enc, cipher, 0);
        if (res != 0) {
            fprintf(stderr, "kcapi_cipher_init(%s) fail: %s\n", cipher, strerror(res));
            ok = false;
        }
        // fprintf(stderr, "IV size: %u\n", kcapi_cipher_ivsize(kch_enc));
    }
    // - For encryption of the IV
    struct kcapi_handle* kch_iv = NULL;
    if (ok && (scheme == SCHEME_AES_CBC_ESSIV_SHA256)) {
        const int res = kcapi_cipher_init(&kch_iv, cipher, 0);
        if (res != 0) {
            fprintf(stderr, "kcapi_cipher_init(%s) fail: %s\n", cipher, strerror(res));
            ok = false;
        }
    }

    // Load key
    while (ok) {
        uint8_t key[64];
        int key_size = 0;
        FILE* fh = fopen(key_file, "rb");
        if (fh == NULL) {
            fprintf(stderr, "Cannot read %s: %s\n", key_file, strerror(errno));
            ok = false;
            break;
        } else {
            const int res = fread(key, 1, sizeof(key), fh);
            if (((scheme == 1) && (res != 32) && (res != 64)) || ((scheme == 2) && (res != 16))) {
                fprintf(stderr, "Bad keys size, %s is %d bytes (%d bits)\n", key_file, res, res * 8);
                ok = false;
            } else {
                key_size = res;
            }
            fclose(fh);
        }

        if (ok && (key_size > 0)) {
            const int res = kcapi_cipher_setkey(kch_enc, key, key_size);
            if (res != 0) {
                fprintf(stderr, "kcapi_cipher_setkey() fail: %s\n", strerror(res));
                ok = false;
            }
        }

        if (ok && (scheme == SCHEME_AES_CBC_ESSIV_SHA256)) {
            uint8_t iv_key[256 / 8];
            const int res = kcapi_md_sha256(key, key_size, iv_key, sizeof(iv_key));
            if (res != sizeof(iv_key)) {
                fprintf(stderr, "kcapi_md_sha256() fail: %s\n", strerror(res));
                ok = false;
            } else {
                // fprintf(stderr, "sha256 %02x %02x %02x %02x ...\n", iv_key[0], iv_key[1], iv_key[2], iv_key[3]);
                const int res2 = kcapi_cipher_setkey(kch_iv, iv_key, sizeof(iv_key));
                if (res2 != 0) {
                    fprintf(stderr, "kcapi_cipher_setkey() fail: %s\n", strerror(res2));
                    ok = false;
                }
            }
        }

        break;
    }

    // Disable libkcapi debugging again, unless user really wants it
    if (verbosity < 2) {
        kcapi_set_verbosity(KCAPI_LOG_WARN);
    }

    // Open input
    FILE* in_fh = NULL;
    size_t in_size = 0;
    size_t in_sectors = 0;
    if (ok) {
        in_fh = fopen(in_file, "rb");
        if (in_fh == NULL) {
            fprintf(stderr, "Cannot read %s: %s\n", in_file, strerror(errno));
            ok = false;
        } else {
            fseek(in_fh, 0, SEEK_END);
            in_size = ftell(in_fh);
            fseek(in_fh, 0, SEEK_SET);
            // File size must be a multiple of the sector size
            if ((in_size % SECTOR_SIZE) != 0) {
                fprintf(stderr, "Unexpected input file size %" PRIuMAX ": not a multiple of %d!\n", in_size,
                        SECTOR_SIZE);
                ok = false;
            }
            // We can (currently...) only handle up to 2^32 sectors
            in_sectors = in_size / SECTOR_SIZE;
            if (in_sectors > UINT32_MAX) {
                fprintf(stderr, "Too large input file, too many sectors: %" PRIuMAX " > %" PRIu32 "!\n", in_sectors,
                        UINT32_MAX);
                ok = false;
            }
        }
    }

    // Open output
    FILE* out_fh = NULL;
    if (ok) {
        out_fh = fopen(out_file, "wb");
        if (out_fh == NULL) {
            fprintf(stderr, "Cannot write %s: %s\n", out_file, strerror(errno));
            ok = false;
        }
    }

    // Encrypt input
    if (ok) {
        fprintf(stderr, "in_file:  %s\n", in_file);
        fprintf(stderr, "out_file: %s\n", out_file);
        fprintf(stderr, "key_file: %s\n", key_file);
        fprintf(stderr, "scheme:   %s\n", SchemeName(scheme));
        fprintf(stderr, "cipher:   %s\n", CipherName(scheme));

        fprintf(stderr, "Encrypting %" PRIuMAX " bytes (%.0f MiB), %" PRIuMAX " sectors...\n", in_size,
                (double)in_size / 1024.0 / 1024.0, in_sectors);

        const bool do_progress = (isatty(fileno(stderr)) == 1);

        // Initialisation vector (IV)
        // uint8_t iv[16];
        // memset(iv, 0, sizeof(iv));

        // Encrypt each sector separately
        for (uint32_t sector_num = 0; ok && (sector_num < in_sectors); sector_num++) {
            // Progress indicator
            if (do_progress && ((sector_num % 10000) == 0)) {
                const double perc = (double)(sector_num + 1) / (double)in_sectors * 1e2;
                fprintf(stderr, "\rSector %u (%.0f%%)", sector_num, perc);
            }

            // Read next sector from input file
            uint8_t sector_data[SECTOR_SIZE];
            if (fread(sector_data, sizeof(sector_data), 1, in_fh) != 1) {
                fprintf(stderr, "Input read fail at sector %u: %s!\n", sector_num, strerror(errno));
                ok = false;
                break;
            }

            // Initialisation vector (IV) is the sector number in little endian uint32_t padded with 0s
            uint8_t iv[16] = {0};
            memcpy(&iv[0], &sector_num, sizeof(sector_num));
            // With ESSIV we encrypt that using E_sha256(K)
            if (scheme == 2) {
                uint8_t nulliv[16] = {0};
                const int res =
                    kcapi_cipher_encrypt(kch_iv, iv, sizeof(iv), nulliv, iv, sizeof(iv), KCAPI_ACCESS_HEURISTIC);
                if (res != sizeof(iv)) {
                    fprintf(stderr, "IV encryption failure at sector %u: %s!\n", sector_num, strerror(res));
                    ok = false;
                    break;
                }
            }

            // Encrypt
            uint8_t encrypted_data[sizeof(sector_data)];
            const int res = kcapi_cipher_encrypt(kch_enc, sector_data, sizeof(sector_data), iv, encrypted_data,
                                                 sizeof(encrypted_data), KCAPI_ACCESS_HEURISTIC);
            if (res != (int)sizeof(encrypted_data)) {
                fprintf(stderr, "Encryption failure at sector %u: %s!\n", sector_num, strerror(res));
                ok = false;
                break;
            }

            // Write to output file
            if (fwrite(encrypted_data, sizeof(encrypted_data), 1, out_fh) != 1) {
                fprintf(stderr, "Output write fail at sector %u: %s!\n", sector_num, strerror(errno));
                ok = false;
                break;
            }
        }

        if (do_progress) {
            fprintf(stderr, "\r                                      \r");
        }
    }

    // Cleanup
    if (in_fh != NULL) {
        fclose(in_fh);
        in_fh = NULL;
    }
    if (out_fh != NULL) {
        fclose(out_fh);
        out_fh = NULL;
    }
    if (kch_enc != NULL) {
        kcapi_cipher_destroy(kch_enc);
        kch_enc = NULL;
    }
    if (kch_iv != NULL) {
        kcapi_cipher_destroy(kch_iv);
        kch_iv = NULL;
    }

    // Are we happy?
    if (ok) {
        fprintf(stderr, "Success :-)\n");
        exit(EXIT_SUCCESS);
    } else {
        fprintf(stderr, "Failure :-(\n");
        exit(EXIT_FAILURE);
    }
}
