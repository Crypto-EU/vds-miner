#include "crypto.hpp"
#include "util.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace {

constexpr uint64_t BLAKE2B_IV[8] = {
    0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL,
    0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL
};

constexpr uint8_t BLAKE2B_SIGMA[12][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
    { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
    { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
    { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 }
};

inline uint64_t rotr64(uint64_t x, unsigned n) { return (x >> n) | (x << (64 - n)); }

inline void blake2b_g(uint64_t v[16], int a, int b, int c, int d, uint64_t x, uint64_t y) {
    v[a] = v[a] + v[b] + x;
    v[d] = rotr64(v[d] ^ v[a], 32);
    v[c] = v[c] + v[d];
    v[b] = rotr64(v[b] ^ v[c], 24);
    v[a] = v[a] + v[b] + y;
    v[d] = rotr64(v[d] ^ v[a], 16);
    v[c] = v[c] + v[d];
    v[b] = rotr64(v[b] ^ v[c], 63);
}

void blake2b_compress(Blake2bState* S, const uint8_t block[128], int last) {
    uint64_t m[16], v[16];
    for (int i = 0; i < 16; ++i) m[i] = load64_le(block + i * 8);
    for (int i = 0; i < 8; ++i) v[i] = S->h[i];
    v[8]  = BLAKE2B_IV[0];
    v[9]  = BLAKE2B_IV[1];
    v[10] = BLAKE2B_IV[2];
    v[11] = BLAKE2B_IV[3];
    v[12] = BLAKE2B_IV[4] ^ S->t[0];
    v[13] = BLAKE2B_IV[5] ^ S->t[1];
    v[14] = BLAKE2B_IV[6] ^ (last ? 0xFFFFFFFFFFFFFFFFULL : 0);
    v[15] = BLAKE2B_IV[7];
    for (int r = 0; r < 12; ++r) {
        const uint8_t* s = BLAKE2B_SIGMA[r];
        blake2b_g(v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
        blake2b_g(v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
        blake2b_g(v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
        blake2b_g(v, 3, 7, 11, 15, m[s[6]], m[s[7]]);
        blake2b_g(v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
        blake2b_g(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
        blake2b_g(v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
        blake2b_g(v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
    }
    for (int i = 0; i < 8; ++i) S->h[i] ^= v[i] ^ v[i + 8];
}

inline void blake2b_increment(Blake2bState* S, uint64_t inc) {
    S->t[0] += inc;
    if (S->t[0] < inc) S->t[1]++;
}

} // namespace

void blake2b_init_equihash96_5(Blake2bState* S) {
    std::memset(S, 0, sizeof(*S));
    for (int i = 0; i < 8; ++i) S->h[i] = BLAKE2B_IV[i];

    // 64-byte BLAKE2b parameter block (libsodium / official blake2.h layout)
    uint8_t p[64] = {};
    p[0] = 60; // digest_length = (512/96)*96/8 = 60
    p[1] = 0;  // key_length
    p[2] = 1;  // fanout
    p[3] = 1;  // depth
    std::memcpy(p + 48, "ZcashPoW", 8);
    uint32_t n = 96, k = 5;
    std::memcpy(p + 56, &n, 4);
    std::memcpy(p + 60, &k, 4);
    for (int i = 0; i < 8; ++i) S->h[i] ^= load64_le(p + i * 8);
}

void blake2b_update(Blake2bState* S, const void* in, size_t inlen) {
    const auto* p = static_cast<const uint8_t*>(in);
    if (S->buflen + inlen < 128) {
        std::memcpy(S->buf + S->buflen, p, inlen);
        S->buflen += inlen;
        return;
    }
    size_t left = 128 - S->buflen;
    std::memcpy(S->buf + S->buflen, p, left);
    blake2b_increment(S, 128);
    blake2b_compress(S, S->buf, 0);
    p += left;
    inlen -= left;
    while (inlen >= 128) {
        blake2b_increment(S, 128);
        blake2b_compress(S, p, 0);
        p += 128;
        inlen -= 128;
    }
    std::memcpy(S->buf, p, inlen);
    S->buflen = inlen;
}

void blake2b_final(Blake2bState* S, void* out, size_t outlen) {
    blake2b_increment(S, S->buflen);
    std::memset(S->buf + S->buflen, 0, 128 - S->buflen);
    blake2b_compress(S, S->buf, 1);
    uint8_t tmp[64];
    for (int i = 0; i < 8; ++i) store64_le(tmp + i * 8, S->h[i]);
    std::memcpy(out, tmp, outlen);
}

// ---- SHA-256 ----

namespace {

constexpr uint32_t SHA_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

inline uint32_t rotr32(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

void sha256_compress(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = load32_be(block + i * 4);
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + SHA_K[i] + w[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

} // namespace

void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint8_t block[64];
    size_t n = len;
    const uint8_t* p = data;
    while (n >= 64) {
        sha256_compress(state, p);
        p += 64; n -= 64;
    }
    std::memcpy(block, p, n);
    block[n] = 0x80;
    if (n >= 56) {
        std::memset(block + n + 1, 0, 63 - n);
        sha256_compress(state, block);
        std::memset(block, 0, 56);
    } else {
        std::memset(block + n + 1, 0, 55 - n);
    }
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; ++i) block[63 - i] = (uint8_t)(bits >> (8 * i));
    sha256_compress(state, block);
    for (int i = 0; i < 8; ++i) store32_be(out + i * 4, state[i]);
}

void hmac_sha256(const uint8_t* key, size_t keylen, const uint8_t* data, size_t datalen, uint8_t out[32]) {
    uint8_t kpad[64] = {};
    if (keylen > 64) {
        sha256(key, keylen, kpad);
    } else {
        std::memcpy(kpad, key, keylen);
    }
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = kpad[i] ^ 0x36;
        opad[i] = kpad[i] ^ 0x5c;
    }
    std::vector<uint8_t> inner(64 + datalen);
    std::memcpy(inner.data(), ipad, 64);
    std::memcpy(inner.data() + 64, data, datalen);
    uint8_t inner_hash[32];
    sha256(inner.data(), inner.size(), inner_hash);
    uint8_t outer[96];
    std::memcpy(outer, opad, 64);
    std::memcpy(outer + 64, inner_hash, 32);
    sha256(outer, 96, out);
}

void pbkdf2_hmac_sha256(const uint8_t* pw, size_t pwlen, const uint8_t* salt, size_t saltlen,
                        uint32_t c, uint8_t* dk, size_t dklen) {
    uint32_t blocks = (uint32_t)((dklen + 31) / 32);
    std::vector<uint8_t> asalt(saltlen + 4);
    std::memcpy(asalt.data(), salt, saltlen);
    uint8_t u[32], t[32];
    size_t offset = 0;
    for (uint32_t i = 1; i <= blocks; ++i) {
        store32_be(asalt.data() + saltlen, i);
        hmac_sha256(pw, pwlen, asalt.data(), asalt.size(), u);
        std::memcpy(t, u, 32);
        for (uint32_t j = 1; j < c; ++j) {
            hmac_sha256(pw, pwlen, u, 32, u);
            for (int k = 0; k < 32; ++k) t[k] ^= u[k];
        }
        size_t take = std::min<size_t>(32, dklen - offset);
        std::memcpy(dk + offset, t, take);
        offset += take;
    }
}

namespace {

inline uint32_t rotl32(uint32_t a, unsigned b) { return (a << b) | (a >> (32 - b)); }

void xor_salsa8(uint32_t B[16], const uint32_t Bx[16]) {
    uint32_t x00 = (B[0] ^= Bx[0]);
    uint32_t x01 = (B[1] ^= Bx[1]);
    uint32_t x02 = (B[2] ^= Bx[2]);
    uint32_t x03 = (B[3] ^= Bx[3]);
    uint32_t x04 = (B[4] ^= Bx[4]);
    uint32_t x05 = (B[5] ^= Bx[5]);
    uint32_t x06 = (B[6] ^= Bx[6]);
    uint32_t x07 = (B[7] ^= Bx[7]);
    uint32_t x08 = (B[8] ^= Bx[8]);
    uint32_t x09 = (B[9] ^= Bx[9]);
    uint32_t x10 = (B[10] ^= Bx[10]);
    uint32_t x11 = (B[11] ^= Bx[11]);
    uint32_t x12 = (B[12] ^= Bx[12]);
    uint32_t x13 = (B[13] ^= Bx[13]);
    uint32_t x14 = (B[14] ^= Bx[14]);
    uint32_t x15 = (B[15] ^= Bx[15]);
    for (int i = 0; i < 8; i += 2) {
        x04 ^= rotl32(x00 + x12, 7); x09 ^= rotl32(x05 + x01, 7);
        x14 ^= rotl32(x10 + x06, 7); x03 ^= rotl32(x15 + x11, 7);
        x08 ^= rotl32(x04 + x00, 9); x13 ^= rotl32(x09 + x05, 9);
        x02 ^= rotl32(x14 + x10, 9); x07 ^= rotl32(x03 + x15, 9);
        x12 ^= rotl32(x08 + x04, 13); x01 ^= rotl32(x13 + x09, 13);
        x06 ^= rotl32(x02 + x14, 13); x11 ^= rotl32(x07 + x03, 13);
        x00 ^= rotl32(x12 + x08, 18); x05 ^= rotl32(x01 + x13, 18);
        x10 ^= rotl32(x06 + x02, 18); x15 ^= rotl32(x11 + x07, 18);
        x01 ^= rotl32(x00 + x03, 7); x06 ^= rotl32(x05 + x04, 7);
        x11 ^= rotl32(x10 + x09, 7); x12 ^= rotl32(x15 + x14, 7);
        x02 ^= rotl32(x01 + x00, 9); x07 ^= rotl32(x06 + x05, 9);
        x08 ^= rotl32(x11 + x10, 9); x13 ^= rotl32(x12 + x15, 9);
        x03 ^= rotl32(x02 + x01, 13); x04 ^= rotl32(x07 + x06, 13);
        x09 ^= rotl32(x08 + x11, 13); x14 ^= rotl32(x13 + x12, 13);
        x00 ^= rotl32(x03 + x02, 18); x05 ^= rotl32(x04 + x07, 18);
        x10 ^= rotl32(x09 + x08, 18); x15 ^= rotl32(x14 + x13, 18);
    }
    B[0] += x00; B[1] += x01; B[2] += x02; B[3] += x03;
    B[4] += x04; B[5] += x05; B[6] += x06; B[7] += x07;
    B[8] += x08; B[9] += x09; B[10] += x10; B[11] += x11;
    B[12] += x12; B[13] += x13; B[14] += x14; B[15] += x15;
}

} // namespace

void scrypt_1024_1_1_256(const uint8_t input[281], uint8_t output[32]) {
    constexpr int headerLen = 281;
    uint8_t B[128];
    pbkdf2_hmac_sha256(input, headerLen, input, headerLen, 1, B, 128);

    uint32_t X[32];
    for (int k = 0; k < 32; ++k) X[k] = load32_le(B + 4 * k);

    uint32_t V[1024 * 32];
    for (int i = 0; i < 1024; ++i) {
        std::memcpy(&V[i * 32], X, 128);
        xor_salsa8(&X[0], &X[16]);
        xor_salsa8(&X[16], &X[0]);
    }
    for (int i = 0; i < 1024; ++i) {
        uint32_t j = 32 * (X[16] & 1023);
        for (int k = 0; k < 32; ++k) X[k] ^= V[j + k];
        xor_salsa8(&X[0], &X[16]);
        xor_salsa8(&X[16], &X[0]);
    }
    for (int k = 0; k < 32; ++k) store32_le(B + 4 * k, X[k]);
    pbkdf2_hmac_sha256(input, headerLen, B, 128, 1, output, 32);
}
