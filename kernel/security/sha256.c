/* AURELIA OS - SHA-256 (FIPS 180-4) */
#include <aurelia/sha256.h>
#include <aurelia/kstring.h>

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define BSIG1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SSIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SSIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static const u32 k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static void transform(u32 state[8], const u8 block[64])
{
    u32 w[64];
    for (i32 i = 0; i < 16; i++) {
        w[i] = ((u32)block[i * 4] << 24) | ((u32)block[i * 4 + 1] << 16) |
               ((u32)block[i * 4 + 2] << 8) | (u32)block[i * 4 + 3];
    }
    for (i32 i = 16; i < 64; i++) {
        w[i] = SSIG1(w[i - 2]) + w[i - 7] + SSIG0(w[i - 15]) + w[i - 16];
    }
    u32 a = state[0], b = state[1], c = state[2], d = state[3];
    u32 e = state[4], f = state[5], g = state[6], h = state[7];
    for (i32 i = 0; i < 64; i++) {
        u32 t1 = h + BSIG1(e) + CH(e, f, g) + k[i] + w[i];
        u32 t2 = BSIG0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void sha256_init(sha256_ctx_t *ctx)
{
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

void sha256_update(sha256_ctx_t *ctx, const void *data, size_t length)
{
    const u8 *bytes = data;
    ctx->bit_count += (u64)length * 8;
    while (length > 0) {
        u32 space = 64 - ctx->buffer_len;
        u32 take = (u32)(length < space ? length : space);
        memcpy(ctx->buffer + ctx->buffer_len, bytes, take);
        ctx->buffer_len += take;
        bytes += take;
        length -= take;
        if (ctx->buffer_len == 64) {
            transform(ctx->state, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

void sha256_final(sha256_ctx_t *ctx, u8 digest[32])
{
    u64 bits = ctx->bit_count;
    u8 pad = 0x80;
    sha256_update(ctx, &pad, 1);
    u8 zero = 0;
    while (ctx->buffer_len != 56) {
        sha256_update(ctx, &zero, 1);
    }
    u8 length_bytes[8];
    for (i32 i = 0; i < 8; i++) {
        length_bytes[7 - i] = (u8)(bits >> (i * 8));
    }
    sha256_update(ctx, length_bytes, 8);
    for (i32 i = 0; i < 8; i++) {
        digest[i * 4] = (u8)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (u8)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (u8)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (u8)(ctx->state[i]);
    }
}

void sha256_hex(const void *data, size_t length, char out[65])
{
    static const char hex[] = "0123456789abcdef";
    u8 digest[32];
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, length);
    sha256_final(&ctx, digest);
    for (i32 i = 0; i < 32; i++) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    out[64] = '\0';
}
