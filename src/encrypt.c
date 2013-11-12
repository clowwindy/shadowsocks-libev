#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

#if defined(USE_CRYPTO_OPENSSL)

#include <openssl/md5.h>
#include <openssl/rand.h>

#elif defined(USE_CRYPTO_POLARSSL)

#include <polarssl/md5.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/version.h>
#define CIPHER_UNSUPPORTED "unsupported"

#endif

#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <stdio.h>
#endif

#include "encrypt.h"
#include "utils.h"

#define OFFSET_ROL(p, o) ((uint64_t)(*(p + o)) << (8 * o))

static uint8_t *enc_table;
static uint8_t *dec_table;
static uint8_t enc_key[MAX_KEY_LENGTH];
static int enc_key_len;
static int enc_iv_len;

#ifdef DEBUG
static void dump(char *tag, char *text)
{
    int i, len;
    len = strlen(text);
    printf("%s: ", tag);
    for (i = 0; i < len; i++)
    {
        printf("0x%02x ", (uint8_t)text[i]);
    }
    printf("\n");
}
#endif

static const char* supported_ciphers[CIPHER_NUM] =
{
    "table",
    "rc4",
    "aes-128-cfb",
    "aes-192-cfb",
    "aes-256-cfb",
    "bf-cfb",
    "camellia-128-cfb",
    "camellia-192-cfb",
    "camellia-256-cfb",
    "cast5-cfb",
    "des-cfb",
    "idea-cfb",
    "rc2-cfb",
    "seed-cfb"
};

#ifdef USE_CRYPTO_POLARSSL
static const char* supported_ciphers_polarssl[CIPHER_NUM] =
{
    "table",
    "ARC4-128",
    "AES-128-CFB128",
    "AES-192-CFB128",
    "AES-256-CFB128",
    "BLOWFISH-CFB64",
    "CAMELLIA-128-CFB128",
    "CAMELLIA-192-CFB128",
    "CAMELLIA-256-CFB128",
    CIPHER_UNSUPPORTED,
    CIPHER_UNSUPPORTED,
    CIPHER_UNSUPPORTED,
    CIPHER_UNSUPPORTED,
    CIPHER_UNSUPPORTED
};
#endif

static int random_compare(const void *_x, const void *_y, uint32_t i, uint64_t a)
{
    uint8_t x = *((uint8_t *) _x);
    uint8_t y = *((uint8_t*) _y);
    return (a % (x + i) - a % (y + i));
}

static void merge(uint8_t *left, int llength, uint8_t *right,
                  int rlength, uint32_t salt, uint64_t key)
{
    uint8_t *ltmp = (uint8_t *) malloc(llength * sizeof(uint8_t));
    uint8_t *rtmp = (uint8_t *) malloc(rlength * sizeof(uint8_t));

    uint8_t *ll = ltmp;
    uint8_t *rr = rtmp;

    uint8_t *result = left;

    memcpy(ltmp, left, llength * sizeof(uint8_t));
    memcpy(rtmp, right, rlength * sizeof(uint8_t));

    while (llength > 0 && rlength > 0)
    {
        if (random_compare(ll, rr, salt, key) <= 0)
        {
            *result = *ll;
            ++ll;
            --llength;
        }
        else
        {
            *result = *rr;
            ++rr;
            --rlength;
        }
        ++result;
    }

    if (llength > 0)
        while (llength > 0)
        {
            *result = *ll;
            ++result;
            ++ll;
            --llength;
        }
    else
        while (rlength > 0)
        {
            *result = *rr;
            ++result;
            ++rr;
            --rlength;
        }

    free(ltmp);
    free(rtmp);
}

static void merge_sort(uint8_t array[], int length,
                       uint32_t salt, uint64_t key)
{
    uint8_t middle;
    uint8_t *left, *right;
    int llength;

    if (length <= 1)
        return;

    middle = length / 2;

    llength = length - middle;

    left = array;
    right = array + llength;

    merge_sort(left, llength, salt, key);
    merge_sort(right, middle, salt, key);
    merge(left, llength, right, middle, salt, key);
}

int enc_get_iv_len()
{
    return enc_iv_len;
}

unsigned char *enc_md5(const unsigned char *d, size_t n, unsigned char *md)
{
#if defined(USE_CRYPTO_OPENSSL)
    return MD5(d, n, md);
#elif defined(USE_CRYPTO_POLARSSL)
    static unsigned char m[16];
    if (md == NULL) {
        md = m;
    }
    md5(d, n, md);
    return md;
#endif
}

void enc_table_init(const char *pass)
{
    uint32_t i;
    uint32_t salt;
    uint64_t key = 0;
    uint8_t *digest;

    enc_table = malloc(256);
    dec_table = malloc(256);

    digest = enc_md5((const uint8_t *)pass, strlen(pass), NULL);

    for (i = 0; i < 8; i++)
    {
        key += OFFSET_ROL(digest, i);
    }

    for(i = 0; i < 256; ++i)
    {
        enc_table[i] = i;
    }
    for(i = 1; i < 1024; ++i)
    {
        salt = i;
        merge_sort(enc_table, 256, salt, key);
    }
    for(i = 0; i < 256; ++i)
    {
        // gen decrypt table from encrypt table
        dec_table[enc_table[i]] = i;
    }
}

int cipher_iv_size(const cipher_kt_t *cipher)
{
#if defined(USE_CRYPTO_OPENSSL)
    return EVP_CIPHER_iv_length (cipher);
#elif defined(USE_CRYPTO_POLARSSL)
    if (cipher == NULL) {
        return 0;
    }
    return cipher->iv_size;
#endif
}

int cipher_key_size (const cipher_kt_t *cipher)
{
#if defined(USE_CRYPTO_OPENSSL)
    return EVP_CIPHER_key_length(cipher);
#elif defined(USE_CRYPTO_POLARSSL)
    if (cipher == NULL) {
        return 0;
    }
    /* Override PolarSSL 32 bit default key size with sane 128 bit default */
    if (POLARSSL_CIPHER_ID_BLOWFISH == cipher->base->cipher) {
        return 128 / 8;
    }
    return cipher->key_length / 8;
#endif
}

int bytes_to_key(const cipher_kt_t *cipher, const digest_type_t *md, const uint8_t *pass, uint8_t *key, uint8_t *iv)
{
    size_t datal;
    datal = strlen((const char *) pass);
#if defined(USE_CRYPTO_OPENSSL)
    return EVP_BytesToKey(cipher, md, NULL, pass, datal, 1, key, iv);
#elif defined(USE_CRYPTO_POLARSSL)
    md_context_t c;
    unsigned char md_buf[MAX_MD_SIZE];
    int niv;
    int nkey;
    int addmd;
    unsigned int mds;
    unsigned int i;
    int rv;

    nkey = cipher_key_size(cipher);
    niv = cipher_iv_size(cipher);
    rv = nkey;
    if (pass == NULL) {
        return nkey;
    }

    memset(&c, 0, sizeof(md_context_t));
    if (md_init_ctx(&c, md)) {
        return 0;
    }
    addmd = 0;
    mds = md_get_size(md);
    for (;;) {
        int error;
        do {
            error = 1;
            if (md_starts(&c)) {
                break;
            }
            if (addmd) {
                if (md_update(&c, &(md_buf[0]), mds)) {
                    break;
                }  
            } else {
                addmd = 1;
            }
            if (md_update(&c, pass, datal))
                break;
            if (md_finish(&c, &(md_buf[0])))
                break;
            error = 0;
        } while (0);
        if (error) {
            md_free_ctx(&c);
            memset(md_buf, 0, MAX_MD_SIZE);
            return 0;
        }

        i=0;
        if (nkey) {
            for (;;) {
                if (nkey == 0) break;
                if (i == mds) break;
                if (key != NULL)
                    *(key++)=md_buf[i];
                nkey--;
                i++;
            }
        }
        if (niv && (i != mds)) {
            for (;;) {
                if (niv == 0) break;
                if (i == mds) break;
                if (iv != NULL)
                    *(iv++)=md_buf[i];
                niv--;
                i++;
            }
        }
        if ((nkey == 0) && (niv == 0)) break;
    }
    md_free_ctx(&c);
    memset(md_buf, 0, MAX_MD_SIZE);
    return rv;
#endif
}

int rand_bytes(uint8_t *output, int len)
{
#if defined(USE_CRYPTO_OPENSSL)
    return RAND_bytes(output, len);
#elif defined(USE_CRYPTO_POLARSSL)
    static entropy_context ec = {0};
    static ctr_drbg_context cd_ctx = {0};
    static unsigned char rand_initialised = 0;
    const size_t blen = min(len, CTR_DRBG_MAX_REQUEST);

    if (!rand_initialised) {
#ifdef _WIN32
        HCRYPTPROV hProvider;
        union {
            unsigned __int64 seed;
            BYTE buffer[8];
        } rand_buffer;

        hProvider = 0;
        if (CryptAcquireContext(&hProvider, 0, 0, PROV_RSA_FULL, \
                                CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
            CryptGenRandom(hProvider, 8, rand_buffer.buffer);
            CryptReleaseContext(hProvider, 0);
        } else {
            rand_buffer.seed = (unsigned __int64) clock();
        }
#else
        FILE *urand;
        union {
            uint64_t seed;
            uint8_t buffer[8];
        } rand_buffer;

        urand = fopen("/dev/urandom", "r");
        if (urand) {
            fread(&rand_buffer.seed, sizeof(rand_buffer.seed), 1, urand);
            fclose(urand);
        } else {
            rand_buffer.seed = (uint64_t) clock();
        }
#endif
        entropy_init(&ec);
        if (ctr_drbg_init(&cd_ctx, entropy_func, &ec, (const unsigned char *) rand_buffer.buffer, 8) != 0) {
#if POLARSSL_VERSION_NUMBER >= 0x01030000
            entropy_free(&ec);
#endif
            FATAL("Failed to initialize random generator");
        }
        rand_initialised = 1;
    }
#ifdef DEBUG
    int orig_len = len;
    uint8_t *orig_output = output;
#endif
    while (len > 0) {
        if (ctr_drbg_random(&cd_ctx, output, blen) != 0) {
            return 0;
        }
        output += blen;
        len -= blen;
    }
    return 1;
#endif
}

const cipher_kt_t *get_cipher_type(int method)
{
    if (method <= TABLE || method >= CIPHER_NUM) {
        LOGE("get_cipher_type(): Illegal method");
        return NULL;
    }

    const char *ciphername = supported_ciphers[method];
#if defined(USE_CRYPTO_OPENSSL)
    return EVP_get_cipherbyname(ciphername);
#elif defined(USE_CRYPTO_POLARSSL)
    const char *polarname = supported_ciphers_polarssl[method];
    if (strcmp(polarname, CIPHER_UNSUPPORTED) == 0) {
        LOGE("Cipher %s currently is not supported by PolarSSL library", ciphername);
        FATAL("Unsupported cipher in PolarSSL");
    }
    return cipher_info_from_string(polarname);
#endif
}

const digest_type_t *get_digest_type(const char *digest)
{
    if (digest == NULL) {
        LOGE("get_digest_type(): Digest name is null");
        return NULL;
    }

#if defined(USE_CRYPTO_OPENSSL)
    return EVP_get_digestbyname(digest);
#elif defined(USE_CRYPTO_POLARSSL)
    return md_info_from_string(digest);
#endif
}

void cipher_context_init(cipher_ctx_t *evp, int method, int enc)
{
    if (method <= TABLE || method >= CIPHER_NUM) {
        LOGE("cipher_context_init(): Illegal method");
        return;
    }

    const char *ciphername = supported_ciphers[method];
    const cipher_kt_t *cipher = get_cipher_type(method);
#if defined(USE_CRYPTO_OPENSSL)
    if (cipher == NULL) {
        LOGE("Cipher %s not found in OpenSSL library", ciphername);
        FATAL("Cannot initialize cipher");
    }
    EVP_CIPHER_CTX_init(evp);
    if (!EVP_CipherInit_ex(evp, cipher, NULL, NULL, NULL, enc)) {
        LOGE("Cannot initialize cipher %s", ciphername);
        exit(EXIT_FAILURE);
    }
    if (!EVP_CIPHER_CTX_set_key_length(evp, enc_key_len)) {
        EVP_CIPHER_CTX_cleanup(evp);
        LOGE("Invalid key length: %d", enc_key_len);
        exit(EXIT_FAILURE);
    }
    if (method > RC4) {
        EVP_CIPHER_CTX_set_padding(evp, 1);
    }
#elif defined(USE_CRYPTO_POLARSSL)
    if (cipher == NULL) {
        LOGE("Cipher %s not found in PolarSSL library", ciphername);
        FATAL("Cannot initialize PolarSSL cipher");
    }
    if (cipher_init_ctx(evp, cipher) != 0) {
        FATAL("Cannot initialize PolarSSL cipher context");
    }
#endif
}

void cipher_context_set_iv(cipher_ctx_t *evp, uint8_t *iv, size_t iv_len, int enc)
{
    if (evp == NULL || iv == NULL) {
        LOGE("cipher_context_set_keyiv(): Cipher context or IV is null");
        return;
    }
    if (enc) {
        rand_bytes(iv, iv_len);
    }
#if defined(USE_CRYPTO_OPENSSL)
    if (!EVP_CipherInit_ex(evp, NULL, NULL, enc_key, iv, enc)) {
        EVP_CIPHER_CTX_cleanup(evp);
        FATAL("Cannot set key and IV");
    }
#elif defined(USE_CRYPTO_POLARSSL)
    if (cipher_setkey(evp, enc_key, enc_key_len * 8, enc) != 0) {
        cipher_free_ctx(evp);
        FATAL("Cannot set PolarSSL cipher key");
    }
#if POLARSSL_VERSION_NUMBER >= 0x01030000
    if (cipher_set_iv(evp, iv, iv_len) != 0) {
        cipher_free_ctx(evp);
        FATAL("Cannot set PolarSSL cipher IV");
    }
    if(cipher_reset(evp) != 0) {
        cipher_free_ctx(evp);
        FATAL("Cannot finalize PolarSSL cipher context");
    }
#else
    if(cipher_reset(evp, iv) != 0) {
        cipher_free_ctx(evp);
        FATAL("Cannot set PolarSSL cipher IV");
    }
#endif
#endif
#ifdef DEBUG
    dump("IV", iv);
#endif
}

void cipher_context_release(cipher_ctx_t *evp) {
#if defined(USE_CRYPTO_OPENSSL)
    EVP_CIPHER_CTX_cleanup(evp);
#elif defined(USE_CRYPTO_POLARSSL)
    cipher_free_ctx(evp);
#endif
}

int cipher_context_update(cipher_ctx_t *evp, uint8_t *output, int *olen, \
                          const uint8_t *input, int ilen) {
#if defined(USE_CRYPTO_OPENSSL)
        return EVP_CipherUpdate(evp, (uint8_t *) output, (size_t *) olen, \
                                (const uint8_t *) input, (size_t) ilen);
#elif defined(USE_CRYPTO_POLARSSL)
        return !cipher_update(evp, (const uint8_t *) input, (size_t) ilen, \
                              (uint8_t *) output, (size_t *) olen);
#endif
}

char* ss_encrypt_all(int buf_size, char *plaintext, ssize_t *len, int method)
{
    if (method > TABLE)
    {
        cipher_ctx_t evp;
        cipher_context_init(&evp, method, 1);

        int c_len = *len + BLOCK_SIZE;
        int iv_len = 0;
        int err = 0;
        char *ciphertext = malloc(max(iv_len + c_len, buf_size));

        uint8_t iv[MAX_IV_LENGTH];
        iv_len = enc_iv_len;
        cipher_context_set_iv(&evp, iv, iv_len, 1);
        memcpy(ciphertext, iv, iv_len);

        err = cipher_context_update(&evp, (uint8_t*)(ciphertext+iv_len),
                                    &c_len, (const uint8_t *)plaintext, *len);

        if (!err)
        {
            free(ciphertext);
            free(plaintext);
            cipher_context_release(&evp);
            return NULL;
        }

#ifdef DEBUG
        dump("PLAIN", plaintext);
        dump("CIPHER", ciphertext);
#endif

        *len = iv_len + c_len;
        free(plaintext);
        cipher_context_release(&evp);

        return ciphertext;

    }
    else
    {
        char *begin = plaintext;
        while (plaintext < begin + *len)
        {
            *plaintext = (char)enc_table[(uint8_t)*plaintext];
            plaintext++;
        }
        return begin;
    }
}

char* ss_encrypt(int buf_size, char *plaintext, ssize_t *len, struct enc_ctx *ctx)
{
    if (ctx != NULL)
    {
        int c_len = *len + BLOCK_SIZE;
        int iv_len = 0;
        int err = 0;
        char *ciphertext = malloc(max(iv_len + c_len, buf_size));

        if (!ctx->init)
        {
            uint8_t iv[MAX_IV_LENGTH];
            iv_len = enc_iv_len;
            cipher_context_set_iv(&ctx->evp, iv, iv_len, 1);
            memcpy(ciphertext, iv, iv_len);
            ctx->init = 1;
        }

        err = cipher_context_update(&ctx->evp, (uint8_t*)(ciphertext+iv_len),
                                    &c_len, (const uint8_t *)plaintext, *len);
        if (!err)
        {
            free(ciphertext);
            free(plaintext);
            return NULL;
        }

#ifdef DEBUG
        dump("PLAIN", plaintext);
        dump("CIPHER", ciphertext);
#endif

        *len = iv_len + c_len;
        free(plaintext);
        return ciphertext;
    }
    else
    {
        char *begin = plaintext;
        while (plaintext < begin + *len)
        {
            *plaintext = (char)enc_table[(uint8_t)*plaintext];
            plaintext++;
        }
        return begin;
    }
}

char* ss_decrypt_all(int buf_size, char *ciphertext, ssize_t *len, int method)
{
    if (method > TABLE)
    {
        cipher_ctx_t evp;
        cipher_context_init(&evp, method, 0);

        int p_len = *len + BLOCK_SIZE;
        int iv_len = 0;
        int err = 0;
        char *plaintext = malloc(max(p_len, buf_size));

        uint8_t iv[MAX_IV_LENGTH];
        iv_len = enc_iv_len;
        memcpy(iv, ciphertext, iv_len);
        cipher_context_set_iv(&evp, iv, iv_len, 0);

        err = cipher_context_update(&evp, (uint8_t*)plaintext, &p_len,
                                    (const uint8_t*)(ciphertext + iv_len), *len - iv_len);
        if (!err)
        {
            free(ciphertext);
            free(plaintext);
            cipher_context_release(&evp);
            return NULL;
        }

#ifdef DEBUG
        dump("PLAIN", plaintext);
        dump("CIPHER", ciphertext);
#endif

        *len = p_len;
        free(ciphertext);
        cipher_context_release(&evp);
        return plaintext;
    }
    else
    {
        char *begin = ciphertext;
        while (ciphertext < begin + *len)
        {
            *ciphertext = (char)dec_table[(uint8_t)*ciphertext];
            ciphertext++;
        }
        return begin;
    }
}

char* ss_decrypt(int buf_size, char *ciphertext, ssize_t *len, struct enc_ctx *ctx)
{
    if (ctx != NULL)
    {
        int p_len = *len + BLOCK_SIZE;
        int iv_len = 0;
        int err = 0;
        char *plaintext = malloc(max(p_len, buf_size));

        if (!ctx->init)
        {
            uint8_t iv[MAX_IV_LENGTH];
            iv_len = enc_iv_len;
            memcpy(iv, ciphertext, iv_len);
            cipher_context_set_iv(&ctx->evp, iv, iv_len, 0);
            ctx->init = 1;
        }

        err = cipher_context_update(&ctx->evp, (uint8_t*)plaintext, &p_len,
                                    (const uint8_t*)(ciphertext + iv_len), *len - iv_len);

        if (!err)
        {
            free(ciphertext);
            free(plaintext);
            return NULL;
        }

#ifdef DEBUG
        dump("PLAIN", plaintext);
        dump("CIPHER", ciphertext);
#endif

        *len = p_len;
        free(ciphertext);
        return plaintext;
    }
    else
    {
        char *begin = ciphertext;
        while (ciphertext < begin + *len)
        {
            *ciphertext = (char)dec_table[(uint8_t)*ciphertext];
            ciphertext++;
        }
        return begin;
    }
}

void enc_ctx_init(int method, struct enc_ctx *ctx, int enc)
{
    memset(ctx, 0, sizeof(struct enc_ctx));
    cipher_context_init(&ctx->evp, method, enc);
}

void enc_key_init(int method, const char *pass)
{
    if (method <= TABLE || method >= CIPHER_NUM) {
        LOGE("enc_key_init(): Illegal method");
        return;
    }

#if defined(USE_CRYPTO_OPENSSL)
    OpenSSL_add_all_algorithms();
#endif

    uint8_t iv[MAX_IV_LENGTH];
    const cipher_kt_t *cipher = get_cipher_type(method);
    if (cipher == NULL) {
        LOGE("Cipher %s not found in crypto library", supported_ciphers[method]);
        FATAL("Cannot initialize cipher");
    }
    const digest_type_t *md = get_digest_type("MD5");
    if (md == NULL) {
        FATAL("MD5 Digest not found in crypto library");
    }

    enc_key_len = bytes_to_key(cipher, md, (const uint8_t *) pass, enc_key, iv);
    if (enc_key_len == 0) {
        FATAL("Cannot generate key and IV");
    }
    enc_iv_len = cipher_iv_size(cipher);
}

int enc_init(const char *pass, const char *method)
{
    int m = TABLE;
    if (method != NULL)
    {
        for (m = TABLE; m < CIPHER_NUM; m++)
        {
            if (strcmp(method, supported_ciphers[m]) == 0)
            {
                break;
            }
        }
        if (m >= CIPHER_NUM)
        {
            LOGE("Invalid cipher name: %s, use table instead", method);
            m = TABLE;
        }
    }
    if (m == TABLE)
    {
        enc_table_init(pass);
    }
    else
    {
        enc_key_init(m, pass);
    }
    return m;
}

