// Equihash(96,5) hash generation for VDS. BLAKE2b matches libsodium/Zcash.
// Each work-item computes GenerateHash(g) and writes up to 5 x 12-byte hashes.

#define EH_INIT_SIZE 131072
#define EH_INDICES_PER_HASH 5

inline ulong rotr64(ulong x, uint n) { return (x >> n) | (x << (64 - n)); }

constant ulong BLAKE2B_IV[8] = {
    0x6A09E667F3BCC908UL, 0xBB67AE8584CAA73BUL,
    0x3C6EF372FE94F82BUL, 0xA54FF53A5F1D36F1UL,
    0x510E527FADE682D1UL, 0x9B05688C2B3E6C1FUL,
    0x1F83D9ABFB41BD6BUL, 0x5BE0CD19137E2179UL
};

constant uchar BLAKE2B_SIGMA[12][16] = {
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

void blake2b_g(ulong v[16], int a, int b, int c, int d, ulong x, ulong y) {
    v[a] = v[a] + v[b] + x;
    v[d] = rotr64(v[d] ^ v[a], 32);
    v[c] = v[c] + v[d];
    v[b] = rotr64(v[b] ^ v[c], 24);
    v[a] = v[a] + v[b] + y;
    v[d] = rotr64(v[d] ^ v[a], 16);
    v[c] = v[c] + v[d];
    v[b] = rotr64(v[b] ^ v[c], 63);
}

void blake2b_compress(ulong h[8], ulong t0, ulong t1, const uchar block[128], int last) {
    ulong m[16], v[16];
    for (int i = 0; i < 16; ++i) {
        int o = i * 8;
        m[i] = (ulong)block[o] | ((ulong)block[o+1] << 8) | ((ulong)block[o+2] << 16) | ((ulong)block[o+3] << 24)
             | ((ulong)block[o+4] << 32) | ((ulong)block[o+5] << 40) | ((ulong)block[o+6] << 48) | ((ulong)block[o+7] << 56);
    }
    for (int i = 0; i < 8; ++i) v[i] = h[i];
    v[8]  = BLAKE2B_IV[0];
    v[9]  = BLAKE2B_IV[1];
    v[10] = BLAKE2B_IV[2];
    v[11] = BLAKE2B_IV[3];
    v[12] = BLAKE2B_IV[4] ^ t0;
    v[13] = BLAKE2B_IV[5] ^ t1;
    v[14] = BLAKE2B_IV[6] ^ (last ? 0xFFFFFFFFFFFFFFFFUL : 0);
    v[15] = BLAKE2B_IV[7];
    for (int r = 0; r < 12; ++r) {
        blake2b_g(v, 0, 4, 8, 12, m[BLAKE2B_SIGMA[r][0]], m[BLAKE2B_SIGMA[r][1]]);
        blake2b_g(v, 1, 5, 9, 13, m[BLAKE2B_SIGMA[r][2]], m[BLAKE2B_SIGMA[r][3]]);
        blake2b_g(v, 2, 6, 10, 14, m[BLAKE2B_SIGMA[r][4]], m[BLAKE2B_SIGMA[r][5]]);
        blake2b_g(v, 3, 7, 11, 15, m[BLAKE2B_SIGMA[r][6]], m[BLAKE2B_SIGMA[r][7]]);
        blake2b_g(v, 0, 5, 10, 15, m[BLAKE2B_SIGMA[r][8]], m[BLAKE2B_SIGMA[r][9]]);
        blake2b_g(v, 1, 6, 11, 12, m[BLAKE2B_SIGMA[r][10]], m[BLAKE2B_SIGMA[r][11]]);
        blake2b_g(v, 2, 7, 8, 13, m[BLAKE2B_SIGMA[r][12]], m[BLAKE2B_SIGMA[r][13]]);
        blake2b_g(v, 3, 4, 9, 14, m[BLAKE2B_SIGMA[r][14]], m[BLAKE2B_SIGMA[r][15]]);
    }
    for (int i = 0; i < 8; ++i) h[i] ^= v[i] ^ v[i + 8];
}

__kernel void generate_hashes(
    __global const ulong *h_in,     // 8
    const ulong t0,
    const ulong t1,
    __global const uchar *tail,     // 84 bytes leftover of header||nonce
    const uint tail_len,
    __global uchar *out_hashes      // EH_INIT_SIZE * 12
) {
    uint g = get_global_id(0);
    // g runs 0 .. ceil(131072/5)-1 = 0..26214
    const uint max_g = (EH_INIT_SIZE + EH_INDICES_PER_HASH - 1) / EH_INDICES_PER_HASH;
    if (g >= max_g) return;

    ulong h[8];
    for (int i = 0; i < 8; ++i) h[i] = h_in[i];

    uchar block[128];
    for (uint i = 0; i < 128; ++i) block[i] = 0;
    for (uint i = 0; i < tail_len; ++i) block[i] = tail[i];
    // append little-endian g (4 bytes)
    block[tail_len + 0] = (uchar)(g);
    block[tail_len + 1] = (uchar)(g >> 8);
    block[tail_len + 2] = (uchar)(g >> 16);
    block[tail_len + 3] = (uchar)(g >> 24);

    ulong nt0 = t0 + (tail_len + 4);
    ulong nt1 = t1;
    if (nt0 < t0) nt1++;
    blake2b_compress(h, nt0, nt1, block, 1);

    uchar tmp[64];
    for (int i = 0; i < 8; ++i) {
        ulong v = h[i];
        int o = i * 8;
        tmp[o+0] = (uchar)v; tmp[o+1] = (uchar)(v >> 8);
        tmp[o+2] = (uchar)(v >> 16); tmp[o+3] = (uchar)(v >> 24);
        tmp[o+4] = (uchar)(v >> 32); tmp[o+5] = (uchar)(v >> 40);
        tmp[o+6] = (uchar)(v >> 48); tmp[o+7] = (uchar)(v >> 56);
    }

    for (int i = 0; i < EH_INDICES_PER_HASH; ++i) {
        uint idx = g * EH_INDICES_PER_HASH + i;
        if (idx >= EH_INIT_SIZE) break;
        __global uchar *dst = out_hashes + (size_t)idx * 12;
        for (int b = 0; b < 12; ++b) dst[b] = tmp[i * 12 + b];
    }
}
