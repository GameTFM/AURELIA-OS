/* AURELIA OS - SHA-256 (credential hashing) */
#pragma once

#include <aurelia/types.h>

typedef struct {
    u32 state[8];
    u64 bit_count;
    u8 buffer[64];
    u32 buffer_len;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const void *data, size_t length);
void sha256_final(sha256_ctx_t *ctx, u8 digest[32]);
void sha256_hex(const void *data, size_t length, char out[65]);
