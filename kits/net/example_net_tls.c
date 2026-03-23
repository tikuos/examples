/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: TLS 1.3 PSK Crypto Primitives
 *
 * Demonstrates the cryptographic building blocks used by the
 * TLS 1.3 PSK-only client:
 *   - PSK configuration
 *   - HKDF-Extract (early secret derivation)
 *   - HKDF-Expand-Label (key/IV derivation)
 *   - AES-128-GCM encryption and decryption
 *   - GCM authentication tag verification
 *   - TLS key schedule (early -> handshake -> master)
 *   - TLS record layer (plaintext record construction)
 *   - Finished verify_data computation
 *
 * This example does NOT perform a full TLS handshake (that
 * requires an async TCP connection and callback-driven state
 * machine).  Instead, it exercises the underlying crypto
 * primitives deterministically with known inputs.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "examples/kits/example_kits_printf.h"
#include <stdint.h>
#include <string.h>

#include "tikukits/net/tiku_kits_net.h"

#if defined(HAS_TIKUKITS) && TIKU_KITS_NET_TCP_ENABLE

#include "tikukits/crypto/hkdf/tiku_kits_crypto_hkdf.h"
#include "tikukits/crypto/gcm/tiku_kits_crypto_gcm.h"
#include "tikukits/crypto/tls/tiku_kits_crypto_tls.h"
#include "tikukits/crypto/tls/tiku_kits_crypto_tls_keysched.h"
#include "tikukits/crypto/tls/tiku_kits_crypto_tls_record.h"

/*---------------------------------------------------------------------------*/
/* Helper: check if a buffer is all zeros                                    */
/*---------------------------------------------------------------------------*/

static uint8_t is_all_zero(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        if (buf[i] != 0) {
            return 0;
        }
    }
    return 1;
}

/*---------------------------------------------------------------------------*/
/* Example 1: PSK configuration                                             */
/*---------------------------------------------------------------------------*/

static void example_psk_config(void)
{
    static const uint8_t psk_key[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    static const uint8_t psk_id[] = "tiku_device_01";
    int rc;

    printf("--- PSK Configuration ---\n");

    tiku_kits_crypto_tls_init();
    printf("  tls_init(): OK\n");

    rc = tiku_kits_crypto_tls_set_psk(
        psk_key, sizeof(psk_key),
        psk_id, sizeof(psk_id) - 1);
    printf("  set_psk(): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "PASS" : "FAIL");

    /* NULL key should fail */
    rc = tiku_kits_crypto_tls_set_psk(
        (void *)0, 0, psk_id, sizeof(psk_id) - 1);
    printf("  set_psk(NULL key): %s (expected error)\n",
           rc != TIKU_KITS_CRYPTO_OK ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 2: HKDF-Extract (early secret from PSK)                          */
/*---------------------------------------------------------------------------*/

static void example_hkdf_extract(void)
{
    static const uint8_t psk[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    uint8_t early1[TIKU_KITS_CRYPTO_TLS_HASH_SIZE];
    uint8_t early2[TIKU_KITS_CRYPTO_TLS_HASH_SIZE];
    int rc;

    printf("--- HKDF-Extract (Early Secret) ---\n");

    /* Extract with zero salt (TLS 1.3 PSK early secret) */
    rc = tiku_kits_crypto_hkdf_extract(
        (void *)0, 0, psk, sizeof(psk), early1);
    printf("  hkdf_extract(): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");
    printf("  Non-trivial output: %s\n",
           is_all_zero(early1, sizeof(early1)) == 0
           ? "PASS" : "FAIL");

    /* Deterministic: same PSK -> same result */
    rc = tiku_kits_crypto_hkdf_extract(
        (void *)0, 0, psk, sizeof(psk), early2);
    printf("  Deterministic: %s\n",
           (rc == TIKU_KITS_CRYPTO_OK &&
            memcmp(early1, early2, sizeof(early1)) == 0)
           ? "PASS" : "FAIL");

    printf("  First 8 bytes: "
           "%02X %02X %02X %02X %02X %02X %02X %02X\n",
           early1[0], early1[1], early1[2], early1[3],
           early1[4], early1[5], early1[6], early1[7]);
}

/*---------------------------------------------------------------------------*/
/* Example 3: HKDF-Expand-Label (key and IV derivation)                      */
/*---------------------------------------------------------------------------*/

static void example_hkdf_expand_label(void)
{
    /* Use a dummy secret (32 bytes) */
    static const uint8_t secret[32] = {
        0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
        0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C
    };
    uint8_t key[TIKU_KITS_CRYPTO_TLS_KEY_SIZE];
    uint8_t iv[TIKU_KITS_CRYPTO_TLS_IV_SIZE];
    int rc;

    printf("--- HKDF-Expand-Label (Key + IV) ---\n");

    /* Derive 16-byte key with label "key" */
    rc = tiku_kits_crypto_hkdf_expand_label(
        secret, "key", (void *)0, 0,
        key, TIKU_KITS_CRYPTO_TLS_KEY_SIZE);
    printf("  expand_label(\"key\", 16B): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");
    printf("  Key non-trivial: %s\n",
           is_all_zero(key, sizeof(key)) == 0
           ? "PASS" : "FAIL");

    /* Derive 12-byte IV with label "iv" */
    rc = tiku_kits_crypto_hkdf_expand_label(
        secret, "iv", (void *)0, 0,
        iv, TIKU_KITS_CRYPTO_TLS_IV_SIZE);
    printf("  expand_label(\"iv\", 12B): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");
    printf("  IV non-trivial: %s\n",
           is_all_zero(iv, sizeof(iv)) == 0
           ? "PASS" : "FAIL");

    printf("  Key: %02X %02X %02X %02X ... (%u bytes)\n",
           key[0], key[1], key[2], key[3],
           (unsigned)sizeof(key));
    printf("  IV:  %02X %02X %02X %02X ... (%u bytes)\n",
           iv[0], iv[1], iv[2], iv[3],
           (unsigned)sizeof(iv));
}

/*---------------------------------------------------------------------------*/
/* Example 4: AES-128-GCM encrypt and decrypt                               */
/*---------------------------------------------------------------------------*/

static void example_gcm_encrypt_decrypt(void)
{
    static const uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    static const uint8_t nonce[12] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B
    };
    static const uint8_t plaintext[] = "Hello TLS!";
    uint8_t ct[sizeof(plaintext) - 1];
    uint8_t tag[TIKU_KITS_CRYPTO_GCM_TAG_SIZE];
    uint8_t pt_out[sizeof(plaintext) - 1];
    tiku_kits_crypto_gcm_ctx_t ctx;
    int rc;

    printf("--- AES-128-GCM Encrypt/Decrypt ---\n");

    rc = tiku_kits_crypto_gcm_init(&ctx, key);
    printf("  gcm_init(): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");

    /* Encrypt */
    rc = tiku_kits_crypto_gcm_encrypt(
        &ctx, nonce, (void *)0, 0,
        plaintext, sizeof(plaintext) - 1,
        ct, tag);
    printf("  gcm_encrypt(\"Hello TLS!\"): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");
    printf("  Ciphertext differs from plaintext: %s\n",
           memcmp(ct, plaintext, sizeof(ct)) != 0
           ? "PASS" : "FAIL");

    /* Decrypt */
    rc = tiku_kits_crypto_gcm_decrypt(
        &ctx, nonce, (void *)0, 0,
        ct, sizeof(ct), tag, pt_out);
    printf("  gcm_decrypt(): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");
    printf("  Recovered plaintext: \"%.*s\" -> %s\n",
           (int)(sizeof(pt_out)), (const char *)pt_out,
           memcmp(pt_out, plaintext, sizeof(pt_out)) == 0
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 5: GCM tag verification (tampered ciphertext)                     */
/*---------------------------------------------------------------------------*/

static void example_gcm_tag_verify(void)
{
    static const uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    static const uint8_t nonce[12] = {
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
        0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B
    };
    static const uint8_t plaintext[] = "Integrity!";
    uint8_t ct[sizeof(plaintext) - 1];
    uint8_t tag[TIKU_KITS_CRYPTO_GCM_TAG_SIZE];
    uint8_t pt_out[sizeof(plaintext) - 1];
    tiku_kits_crypto_gcm_ctx_t ctx;
    int rc;

    printf("--- GCM Tag Verification ---\n");

    tiku_kits_crypto_gcm_init(&ctx, key);
    tiku_kits_crypto_gcm_encrypt(
        &ctx, nonce, (void *)0, 0,
        plaintext, sizeof(plaintext) - 1,
        ct, tag);

    /* Flip a bit in the ciphertext */
    ct[0] ^= 0x01;

    rc = tiku_kits_crypto_gcm_decrypt(
        &ctx, nonce, (void *)0, 0,
        ct, sizeof(ct), tag, pt_out);
    printf("  Tampered ciphertext decrypt: %s\n",
           rc == TIKU_KITS_CRYPTO_ERR_CORRUPT
           ? "ERR_CORRUPT (PASS)" : "unexpected (FAIL)");
}

/*---------------------------------------------------------------------------*/
/* Example 6: TLS key schedule (PSK -> early -> hs -> master)                */
/*---------------------------------------------------------------------------*/

static void example_key_schedule(void)
{
    static const uint8_t psk[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    uint8_t early[TIKU_KITS_CRYPTO_TLS_HASH_SIZE];
    uint8_t hs[TIKU_KITS_CRYPTO_TLS_HASH_SIZE];
    uint8_t master[TIKU_KITS_CRYPTO_TLS_HASH_SIZE];
    int rc;

    printf("--- TLS Key Schedule ---\n");

    /* Step 1: Early Secret from PSK */
    rc = tiku_kits_crypto_tls_early_secret(
        psk, sizeof(psk), early);
    printf("  early_secret(): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");
    printf("  Non-trivial: %s\n",
           is_all_zero(early, sizeof(early)) == 0
           ? "PASS" : "FAIL");

    /* Step 2: Handshake Secret from Early Secret */
    rc = tiku_kits_crypto_tls_handshake_secret(early, hs);
    printf("  handshake_secret(): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");
    printf("  Non-trivial: %s\n",
           is_all_zero(hs, sizeof(hs)) == 0
           ? "PASS" : "FAIL");
    printf("  Differs from early: %s\n",
           memcmp(hs, early, sizeof(hs)) != 0
           ? "PASS" : "FAIL");

    /* Step 3: Master Secret from Handshake Secret */
    rc = tiku_kits_crypto_tls_master_secret(hs, master);
    printf("  master_secret(): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");
    printf("  Non-trivial: %s\n",
           is_all_zero(master, sizeof(master)) == 0
           ? "PASS" : "FAIL");
    printf("  Differs from hs: %s\n",
           memcmp(master, hs, sizeof(master)) != 0
           ? "PASS" : "FAIL");

    printf("  early[0..3]:  %02X %02X %02X %02X\n",
           early[0], early[1], early[2], early[3]);
    printf("  hs[0..3]:     %02X %02X %02X %02X\n",
           hs[0], hs[1], hs[2], hs[3]);
    printf("  master[0..3]: %02X %02X %02X %02X\n",
           master[0], master[1], master[2], master[3]);
}

/*---------------------------------------------------------------------------*/
/* Example 7: TLS record layer (plaintext record)                            */
/*---------------------------------------------------------------------------*/

static void example_record_layer(void)
{
    /* Simulate a small ClientHello-sized payload */
    uint8_t content[32];
    uint8_t record[TIKU_KITS_CRYPTO_TLS_RECORD_BUF_SIZE];
    uint16_t rec_len;
    uint16_t i;

    printf("--- TLS Record Layer ---\n");

    /* Fill content with a pattern */
    for (i = 0; i < sizeof(content); i++) {
        content[i] = (uint8_t)(i + 1);
    }

    rec_len = tiku_kits_crypto_tls_record_build_plain(
        record,
        TIKU_KITS_CRYPTO_TLS_CT_HANDSHAKE,
        content, sizeof(content));

    printf("  record_build_plain(): %u bytes\n",
           (unsigned)rec_len);
    printf("  Expected length: %u (5 hdr + %u content)\n",
           (unsigned)(5 + sizeof(content)),
           (unsigned)sizeof(content));
    printf("  Length match: %s\n",
           rec_len == 5 + sizeof(content) ? "PASS" : "FAIL");

    /* Verify record header fields */
    {
        uint8_t ct = record[0];
        uint16_t ver = (uint16_t)(
            (uint16_t)record[1] << 8 | record[2]);
        uint16_t frag_len = (uint16_t)(
            (uint16_t)record[3] << 8 | record[4]);

        printf("  Content type: 0x%02X (handshake=0x%02X): %s\n",
               ct, TIKU_KITS_CRYPTO_TLS_CT_HANDSHAKE,
               ct == TIKU_KITS_CRYPTO_TLS_CT_HANDSHAKE
               ? "PASS" : "FAIL");
        printf("  Version: 0x%04X (legacy=0x%04X): %s\n",
               ver, TIKU_KITS_CRYPTO_TLS_LEGACY_VERSION,
               ver == TIKU_KITS_CRYPTO_TLS_LEGACY_VERSION
               ? "PASS" : "FAIL");
        printf("  Fragment length: %u: %s\n",
               (unsigned)frag_len,
               frag_len == sizeof(content)
               ? "PASS" : "FAIL");
    }

    /* Verify content was copied correctly */
    printf("  Content preserved: %s\n",
           memcmp(record + 5, content, sizeof(content)) == 0
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 8: Finished verify_data                                           */
/*---------------------------------------------------------------------------*/

static void example_finished_verify(void)
{
    /* Dummy handshake traffic secret and transcript hash */
    static const uint8_t base_key[32] = {
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
        0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71, 0x81,
        0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 0x82,
        0x13, 0x23, 0x33, 0x43, 0x53, 0x63, 0x73, 0x83
    };
    static const uint8_t transcript[32] = {
        0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01, 0x02,
        0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1, 0x03, 0x04,
        0xA2, 0xB2, 0xC2, 0xD2, 0xE2, 0xF2, 0x05, 0x06,
        0xA3, 0xB3, 0xC3, 0xD3, 0xE3, 0xF3, 0x07, 0x08
    };
    uint8_t verify1[TIKU_KITS_CRYPTO_TLS_HASH_SIZE];
    uint8_t verify2[TIKU_KITS_CRYPTO_TLS_HASH_SIZE];
    int rc;

    printf("--- Finished verify_data ---\n");

    rc = tiku_kits_crypto_tls_finished_verify(
        base_key, transcript, verify1);
    printf("  finished_verify(): %s\n",
           rc == TIKU_KITS_CRYPTO_OK ? "OK" : "FAIL");

    /* Verify it's 32 bytes and non-trivial */
    printf("  Length: %u bytes -> %s\n",
           (unsigned)sizeof(verify1),
           sizeof(verify1) == TIKU_KITS_CRYPTO_TLS_HASH_SIZE
           ? "PASS" : "FAIL");
    printf("  Non-trivial: %s\n",
           is_all_zero(verify1, sizeof(verify1)) == 0
           ? "PASS" : "FAIL");

    /* Deterministic: same inputs -> same result */
    rc = tiku_kits_crypto_tls_finished_verify(
        base_key, transcript, verify2);
    printf("  Deterministic: %s\n",
           (rc == TIKU_KITS_CRYPTO_OK &&
            memcmp(verify1, verify2, sizeof(verify1)) == 0)
           ? "PASS" : "FAIL");

    printf("  verify[0..7]: "
           "%02X %02X %02X %02X %02X %02X %02X %02X\n",
           verify1[0], verify1[1], verify1[2], verify1[3],
           verify1[4], verify1[5], verify1[6], verify1[7]);
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_net_tls_run(void)
{
    printf("=== TikuKits TLS 1.3 PSK Crypto Examples ===\n\n");

    example_psk_config();
    printf("\n");

    example_hkdf_extract();
    printf("\n");

    example_hkdf_expand_label();
    printf("\n");

    example_gcm_encrypt_decrypt();
    printf("\n");

    example_gcm_tag_verify();
    printf("\n");

    example_key_schedule();
    printf("\n");

    example_record_layer();
    printf("\n");

    example_finished_verify();
    printf("\n");
}

#else /* !HAS_TIKUKITS || !TIKU_KITS_NET_TCP_ENABLE */

void example_net_tls_run(void)
{
    /* TLS not available (requires tikukits + TCP) */
}

#endif
