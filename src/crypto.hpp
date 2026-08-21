#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

// BLAKE2b (RFC 7693 / official BLAKE2 param block used by libsodium & Zcash).
struct Blake2bState {
    uint64_t h[8];
    uint64_t t[2];
    uint64_t f[2];
    uint8_t  buf[128];
    size_t   buflen;
};

void blake2b_init_equihash96_5(Blake2bState* S);
void blake2b_update(Blake2bState* S, const void* in, size_t inlen);
void blake2b_final(Blake2bState* S, void* out, size_t outlen);

void sha256(const uint8_t* data, size_t len, uint8_t out[32]);
void hmac_sha256(const uint8_t* key, size_t keylen, const uint8_t* data, size_t datalen, uint8_t out[32]);
void pbkdf2_hmac_sha256(const uint8_t* pw, size_t pwlen, const uint8_t* salt, size_t saltlen,
                        uint32_t c, uint8_t* dk, size_t dklen);

// VDS PoW: scrypt(N=1024, r=1, p=1, dkLen=32) over the 281-byte serialized header.
void scrypt_1024_1_1_256(const uint8_t input[281], uint8_t output[32]);
