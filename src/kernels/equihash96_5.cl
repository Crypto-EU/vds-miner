// Equihash(96,5) GPU solver for VDS — Blake2b generate + Wagner collisions.
// Host only enqueues the full round pipeline and reads solutions at the end.

#define EH_INIT_SIZE 131072
#define EH_INDICES_PER_HASH 5
#define NBUCKETS 65536
#define SLOT_MAX 12
#define MAX_ITEMS 262144
#define MAX_SOLS 16

typedef struct {
    uchar hash[16];
    uint nidx;
    uint pad[3];
    uint idx[32];
} Item;

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

#define G(a,b,c,d,x,y) \
    do { \
        v[a] = v[a] + v[b] + (x); v[d] = rotr64(v[d] ^ v[a], 32); \
        v[c] = v[c] + v[d];       v[b] = rotr64(v[b] ^ v[c], 24); \
        v[a] = v[a] + v[b] + (y); v[d] = rotr64(v[d] ^ v[a], 16); \
        v[c] = v[c] + v[d];       v[b] = rotr64(v[b] ^ v[c], 63); \
    } while (0)

inline void blake2b_compress(ulong h[8], ulong t0, ulong t1, const ulong m[16], int last) {
    ulong v[16];
    v[0] = h[0]; v[1] = h[1]; v[2] = h[2]; v[3] = h[3];
    v[4] = h[4]; v[5] = h[5]; v[6] = h[6]; v[7] = h[7];
    v[8] = BLAKE2B_IV[0]; v[9] = BLAKE2B_IV[1]; v[10] = BLAKE2B_IV[2]; v[11] = BLAKE2B_IV[3];
    v[12] = BLAKE2B_IV[4] ^ t0; v[13] = BLAKE2B_IV[5] ^ t1;
    v[14] = BLAKE2B_IV[6] ^ (last ? 0xFFFFFFFFFFFFFFFFUL : 0UL);
    v[15] = BLAKE2B_IV[7];
    for (int r = 0; r < 12; ++r) {
        G(0, 4, 8, 12, m[BLAKE2B_SIGMA[r][0]], m[BLAKE2B_SIGMA[r][1]]);
        G(1, 5, 9, 13, m[BLAKE2B_SIGMA[r][2]], m[BLAKE2B_SIGMA[r][3]]);
        G(2, 6, 10, 14, m[BLAKE2B_SIGMA[r][4]], m[BLAKE2B_SIGMA[r][5]]);
        G(3, 7, 11, 15, m[BLAKE2B_SIGMA[r][6]], m[BLAKE2B_SIGMA[r][7]]);
        G(0, 5, 10, 15, m[BLAKE2B_SIGMA[r][8]], m[BLAKE2B_SIGMA[r][9]]);
        G(1, 6, 11, 12, m[BLAKE2B_SIGMA[r][10]], m[BLAKE2B_SIGMA[r][11]]);
        G(2, 7, 8, 13, m[BLAKE2B_SIGMA[r][12]], m[BLAKE2B_SIGMA[r][13]]);
        G(3, 4, 9, 14, m[BLAKE2B_SIGMA[r][14]], m[BLAKE2B_SIGMA[r][15]]);
    }
    h[0] ^= v[0] ^ v[8];  h[1] ^= v[1] ^ v[9];
    h[2] ^= v[2] ^ v[10]; h[3] ^= v[3] ^ v[11];
    h[4] ^= v[4] ^ v[12]; h[5] ^= v[5] ^ v[13];
    h[6] ^= v[6] ^ v[14]; h[7] ^= v[7] ^ v[15];
}

__kernel void generate_hashes(
    __global const ulong *h_in, const ulong t0, const ulong t1,
    __global const uchar *tail, const uint tail_len,
    __global uchar *out_hashes) {
    uint g = get_global_id(0);
    const uint max_g = (EH_INIT_SIZE + EH_INDICES_PER_HASH - 1) / EH_INDICES_PER_HASH;
    if (g >= max_g) return;

    ulong h[8];
    h[0] = h_in[0]; h[1] = h_in[1]; h[2] = h_in[2]; h[3] = h_in[3];
    h[4] = h_in[4]; h[5] = h_in[5]; h[6] = h_in[6]; h[7] = h_in[7];

    ulong m[16];
    #pragma unroll
    for (int i = 0; i < 16; ++i) m[i] = 0UL;
    for (uint i = 0; i < tail_len; ++i) {
        m[i >> 3] |= ((ulong)tail[i]) << ((i & 7) * 8);
    }
    uint o = tail_len;
    m[o >> 3] |= ((ulong)(g & 0xFFu)) << ((o & 7) * 8); o++;
    m[o >> 3] |= ((ulong)((g >> 8) & 0xFFu)) << ((o & 7) * 8); o++;
    m[o >> 3] |= ((ulong)((g >> 16) & 0xFFu)) << ((o & 7) * 8); o++;
    m[o >> 3] |= ((ulong)((g >> 24) & 0xFFu)) << ((o & 7) * 8);

    ulong nt0 = t0 + (tail_len + 4);
    ulong nt1 = t1 + (nt0 < t0 ? 1UL : 0UL);
    blake2b_compress(h, nt0, nt1, m, 1);

    uchar tmp[64];
    #pragma unroll
    for (int i = 0; i < 8; ++i) {
        ulong v = h[i];
        int p = i * 8;
        tmp[p+0]=(uchar)v; tmp[p+1]=(uchar)(v>>8); tmp[p+2]=(uchar)(v>>16); tmp[p+3]=(uchar)(v>>24);
        tmp[p+4]=(uchar)(v>>32); tmp[p+5]=(uchar)(v>>40); tmp[p+6]=(uchar)(v>>48); tmp[p+7]=(uchar)(v>>56);
    }
    for (int i = 0; i < EH_INDICES_PER_HASH; ++i) {
        uint idx = g * EH_INDICES_PER_HASH + i;
        if (idx >= EH_INIT_SIZE) break;
        __global uchar *dst = out_hashes + (size_t)idx * 12;
        #pragma unroll
        for (int b = 0; b < 12; ++b) dst[b] = tmp[i * 12 + b];
    }
}

__kernel void pack_items(__global const uchar *hashes, __global Item *items, const uint n) {
    uint i = get_global_id(0);
    if (i >= n) return;
    Item it;
    #pragma unroll
    for (int b = 0; b < 16; ++b) it.hash[b] = 0;
    #pragma unroll
    for (int b = 0; b < 12; ++b) it.hash[b] = hashes[(size_t)i * 12 + b];
    it.nidx = 1;
    it.pad[0] = it.pad[1] = it.pad[2] = 0;
    it.idx[0] = i;
    #pragma unroll
    for (int k = 1; k < 32; ++k) it.idx[k] = 0;
    items[i] = it;
}

__kernel void zero_u32(__global uint *buf, const uint n) {
    uint i = get_global_id(0);
    if (i < n) buf[i] = 0;
}

// After emit_pairs: nitems = min(out_count, MAX_ITEMS), out_count = 0, buckets zeroed.
__kernel void prepare_round(
    __global uint *out_count,
    __global uint *nitems,
    __global uint *bucket_count) {
    uint i = get_global_id(0);
    if (i == 0) {
        uint n = *out_count;
        if (n > MAX_ITEMS) n = MAX_ITEMS;
        *nitems = n;
        *out_count = 0;
    }
    if (i < NBUCKETS) bucket_count[i] = 0;
}

inline uint collision_key(const Item *it) {
    return ((uint)it->hash[0] << 8) | (uint)it->hash[1];
}

__kernel void fill_buckets(
    __global const Item *items,
    __global const uint *nitems_ptr,
    __global uint *bucket_count,
    __global uint *bucket_slots) {
    uint i = get_global_id(0);
    uint nitems = *nitems_ptr;
    if (nitems > MAX_ITEMS) nitems = MAX_ITEMS;
    if (i >= nitems) return;
    Item it = items[i];
    uint key = collision_key(&it);
    uint slot = atomic_inc(&bucket_count[key]);
    if (slot < SLOT_MAX)
        bucket_slots[key * SLOT_MAX + slot] = i;
}

inline int indices_before(const Item *a, const Item *b, uint nidx) {
    for (uint k = 0; k < nidx; ++k) {
        if (a->idx[k] < b->idx[k]) return 1;
        if (a->idx[k] > b->idx[k]) return 0;
    }
    return 0;
}

inline int distinct_indices(const Item *a, const Item *b, uint nidx) {
    if (nidx == 1) return a->idx[0] != b->idx[0];
    for (uint i = 0; i < nidx; ++i)
        for (uint j = 0; j < nidx; ++j)
            if (a->idx[i] == b->idx[j]) return 0;
    return 1;
}

inline void merge_item(Item *out, const Item *a, const Item *b,
                       uint hash_len, uint nidx, uint trim) {
    #pragma unroll
    for (uint i = 0; i < 16; ++i) out->hash[i] = 0;
    for (uint i = trim; i < hash_len; ++i)
        out->hash[i - trim] = a->hash[i] ^ b->hash[i];
    out->nidx = nidx * 2;
    out->pad[0] = out->pad[1] = out->pad[2] = 0;
    if (indices_before(a, b, nidx)) {
        for (uint k = 0; k < nidx; ++k) {
            out->idx[k] = a->idx[k];
            out->idx[nidx + k] = b->idx[k];
        }
    } else {
        for (uint k = 0; k < nidx; ++k) {
            out->idx[k] = b->idx[k];
            out->idx[nidx + k] = a->idx[k];
        }
    }
    for (uint k = nidx * 2; k < 32; ++k) out->idx[k] = 0;
}

__kernel void emit_pairs(
    __global const Item *in_items,
    __global const uint *bucket_count,
    __global const uint *bucket_slots,
    __global Item *out_items,
    __global uint *out_count,
    const uint hash_len,
    const uint nidx,
    const uint trim) {
    uint bucket = get_global_id(0);
    if (bucket >= NBUCKETS) return;
    uint n = bucket_count[bucket];
    if (n > SLOT_MAX) n = SLOT_MAX;
    if (n < 2) return;

    for (uint l = 0; l + 1 < n; ++l) {
        uint ia = bucket_slots[bucket * SLOT_MAX + l];
        Item a = in_items[ia];
        for (uint m = l + 1; m < n; ++m) {
            uint ib = bucket_slots[bucket * SLOT_MAX + m];
            Item b = in_items[ib];
            if (!distinct_indices(&a, &b, nidx)) continue;
            uint pos = atomic_inc(out_count);
            if (pos >= MAX_ITEMS) continue;
            Item merged;
            merge_item(&merged, &a, &b, hash_len, nidx, trim);
            out_items[pos] = merged;
        }
    }
}

__kernel void emit_final(
    __global const Item *in_items,
    __global const uint *bucket_count,
    __global const uint *bucket_slots,
    __global uint *sols,
    __global uint *sol_count,
    const uint hash_len,
    const uint nidx) {
    uint bucket = get_global_id(0);
    if (bucket >= NBUCKETS) return;
    uint n = bucket_count[bucket];
    if (n > SLOT_MAX) n = SLOT_MAX;
    if (n < 2) return;

    for (uint l = 0; l + 1 < n; ++l) {
        uint ia = bucket_slots[bucket * SLOT_MAX + l];
        Item a = in_items[ia];
        for (uint m = l + 1; m < n; ++m) {
            uint ib = bucket_slots[bucket * SLOT_MAX + m];
            Item b = in_items[ib];
            int zero = 1;
            for (uint t = 0; t < hash_len; ++t) {
                if ((a.hash[t] ^ b.hash[t]) != 0) { zero = 0; break; }
            }
            if (!zero) continue;
            if (!distinct_indices(&a, &b, nidx)) continue;
            uint pos = atomic_inc(sol_count);
            if (pos >= MAX_SOLS) continue;
            Item merged;
            merge_item(&merged, &a, &b, hash_len, nidx, 0);
            for (int k = 0; k < 32; ++k)
                sols[pos * 32 + k] = merged.idx[k];
        }
    }
}
