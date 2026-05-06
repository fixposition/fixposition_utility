/* ___    ___
 * \  \  /  /
 *  \  \/  /   Copyright (c) Fixposition AG
 *  /  /\  \
 * /__/  \__\
 *
 * Tool to create dm-crypt compatible disk images (using OpenSSL)
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#define UNUSED(thing) (void)thing
#define NUMOF(x) (int)(sizeof(x) / sizeof(*(x)))
#define SECTOR_SIZE 512

typedef enum scheme_e {
    SCHEME_UNSPECIFIED = 0,
    SCHEME_AES_XTS_PLAIN = 1,
    SCHEME_AES_CBC_ESSIV_SHA256 = 2,
} scheme_t;

static const char* SchemeName(const scheme_t scheme) {
    switch (scheme) {  // clang-format off
        case SCHEME_UNSPECIFIED:          break;
        case SCHEME_AES_XTS_PLAIN:        return "aes-xts-plain";
        case SCHEME_AES_CBC_ESSIV_SHA256: return "aes-cbc-essiv:sha256";
    }  // clang-format on
    return "?";
}

static const char* CipherName(const scheme_t scheme, int key_size) {
    switch (scheme) {  // clang-format off
        case SCHEME_UNSPECIFIED:          break;
        case SCHEME_AES_XTS_PLAIN:
            return (key_size == 64) ? "aes-256-xts" : "aes-128-xts";
        case SCHEME_AES_CBC_ESSIV_SHA256: return "aes-128-cbc";
    }  // clang-format on
    return "?";
}

static void PrintSslError(const char* what) {
    const unsigned long e = ERR_get_error();
    char buf[256];
    ERR_error_string_n(e, buf, sizeof(buf));
    fprintf(stderr, "%s: %s\n", what, buf);
}

// Encrypt a single block with no padding using the given cipher/key
static bool EncryptOneBlock(const EVP_CIPHER* cipher, const uint8_t* key, int key_size, const uint8_t* iv,
                            const uint8_t* in, int in_len, uint8_t* out) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        PrintSslError("EVP_CIPHER_CTX_new");
        return false;
    }
    bool ok = true;
    if (EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
        PrintSslError("EVP_EncryptInit_ex (cipher)");
        ok = false;
    }
    if (ok && (EVP_CIPHER_CTX_set_key_length(ctx, key_size) != 1)) {
        PrintSslError("EVP_CIPHER_CTX_set_key_length");
        ok = false;
    }
    if (ok && (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1)) {
        PrintSslError("EVP_EncryptInit_ex (key/iv)");
        ok = false;
    }
    if (ok) {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
    }
    int outl = 0;
    if (ok && (EVP_EncryptUpdate(ctx, out, &outl, in, in_len) != 1)) {
        PrintSslError("EVP_EncryptUpdate");
        ok = false;
    }
    int total = outl;
    int finl = 0;
    if (ok && (EVP_EncryptFinal_ex(ctx, out + outl, &finl) != 1)) {
        PrintSslError("EVP_EncryptFinal_ex");
        ok = false;
    }
    total += finl;
    if (ok && (total != in_len)) {
        fprintf(stderr, "Unexpected cipher output length: %d != %d\n", total, in_len);
        ok = false;
    }
    EVP_CIPHER_CTX_free(ctx);
    return ok;
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

    // Initialise OpenSSL error strings (for nicer diagnostics)
    ERR_load_crypto_strings();

    // Load key
    uint8_t key[64];
    int key_size = 0;
    if (ok) {
        FILE* fh = fopen(key_file, "rb");
        if (fh == NULL) {
            fprintf(stderr, "Cannot read %s: %s\n", key_file, strerror(errno));
            ok = false;
        } else {
            const int res = fread(key, 1, sizeof(key), fh);
            if (((scheme == SCHEME_AES_XTS_PLAIN) && (res != 32) && (res != 64)) ||
                ((scheme == SCHEME_AES_CBC_ESSIV_SHA256) && (res != 16))) {
                fprintf(stderr, "Bad keys size, %s is %d bytes (%d bits)\n", key_file, res, res * 8);
                ok = false;
            } else {
                key_size = res;
            }
            fclose(fh);
        }
    }

    // Select ciphers based on scheme and key size
    const EVP_CIPHER* data_cipher = NULL;
    const EVP_CIPHER* iv_cipher = NULL;  // Only used for ESSIV
    uint8_t iv_key[SHA256_DIGEST_LENGTH];
    if (ok) {
        switch (scheme) {
            case SCHEME_AES_XTS_PLAIN:
                // AES-XTS in OpenSSL: key size is 32 bytes -> AES-128-XTS, 64 bytes -> AES-256-XTS
                data_cipher = (key_size == 64) ? EVP_aes_256_xts() : EVP_aes_128_xts();
                break;
            case SCHEME_AES_CBC_ESSIV_SHA256:
                data_cipher = EVP_aes_128_cbc();
                // ESSIV: encrypt the plain IV with AES-CBC keyed by SHA-256(K).
                // For AES-128 data key, SHA-256 of the key is 32 bytes, so we use AES-256-ECB-equivalent
                // by doing a single-block CBC with an all-zero IV (effectively ECB for one block).
                iv_cipher = EVP_aes_256_cbc();
                if (SHA256(key, key_size, iv_key) == NULL) {
                    PrintSslError("SHA256");
                    ok = false;
                }
                break;
            case SCHEME_UNSPECIFIED:
                ok = false;
                break;
        }
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
        fprintf(stderr, "cipher:   %s\n", CipherName(scheme, key_size));

        fprintf(stderr, "Encrypting %" PRIuMAX " bytes (%.0f MiB), %" PRIuMAX " sectors...\n", in_size,
                (double)in_size / 1024.0 / 1024.0, in_sectors);

        const bool do_progress = (isatty(fileno(stderr)) == 1);

        // Pre-create a reusable data-encryption context for efficiency
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (ctx == NULL) {
            PrintSslError("EVP_CIPHER_CTX_new");
            ok = false;
        }

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
            if (scheme == SCHEME_AES_CBC_ESSIV_SHA256) {
                uint8_t nulliv[16] = {0};
                uint8_t enc_iv[16];
                if (!EncryptOneBlock(iv_cipher, iv_key, (int)sizeof(iv_key), nulliv, iv, (int)sizeof(iv), enc_iv)) {
                    fprintf(stderr, "IV encryption failure at sector %u!\n", sector_num);
                    ok = false;
                    break;
                }
                memcpy(iv, enc_iv, sizeof(iv));
            }

            // Encrypt the sector: (re-)init context with the sector IV
            uint8_t encrypted_data[sizeof(sector_data)];
            if (EVP_EncryptInit_ex(ctx, data_cipher, NULL, NULL, NULL) != 1) {
                PrintSslError("EVP_EncryptInit_ex (cipher)");
                ok = false;
                break;
            }
            if (EVP_CIPHER_CTX_set_key_length(ctx, key_size) != 1) {
                PrintSslError("EVP_CIPHER_CTX_set_key_length");
                ok = false;
                break;
            }
            if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
                PrintSslError("EVP_EncryptInit_ex (key/iv)");
                ok = false;
                break;
            }
            EVP_CIPHER_CTX_set_padding(ctx, 0);
            int outl = 0;
            if (EVP_EncryptUpdate(ctx, encrypted_data, &outl, sector_data, (int)sizeof(sector_data)) != 1) {
                PrintSslError("EVP_EncryptUpdate");
                fprintf(stderr, "Encryption failure at sector %u\n", sector_num);
                ok = false;
                break;
            }
            int finl = 0;
            if (EVP_EncryptFinal_ex(ctx, encrypted_data + outl, &finl) != 1) {
                PrintSslError("EVP_EncryptFinal_ex");
                fprintf(stderr, "Encryption failure at sector %u\n", sector_num);
                ok = false;
                break;
            }
            if ((outl + finl) != (int)sizeof(encrypted_data)) {
                fprintf(stderr, "Unexpected ciphertext size at sector %u: %d != %d\n", sector_num, outl + finl,
                        (int)sizeof(encrypted_data));
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

        if (ctx != NULL) {
            EVP_CIPHER_CTX_free(ctx);
        }
    }

    UNUSED(verbosity);

    // Cleanup
    if (in_fh != NULL) {
        fclose(in_fh);
        in_fh = NULL;
    }
    if (out_fh != NULL) {
        fclose(out_fh);
        out_fh = NULL;
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
