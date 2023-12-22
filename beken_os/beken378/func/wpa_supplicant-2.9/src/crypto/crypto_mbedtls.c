#include "includes.h"

#include "common.h"
#include "crypto.h"

#include "mbedtls/md5.h"
#include "mbedtls/des.h"

#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
// #include "mbedtls/pkcs5.h"
#include "mbedtls/compat-2.x.h"
#include "mbedtls/aes.h"
#include "mbedtls/rsa.h"
//#include "mbedtls/arc4.h"

// #include "system.h"

#define TAG "CRYPTO_MBEDTLS"

static int mbedtls_hmac_vector(mbedtls_md_type_t type, const u8 *key,
			size_t key_len, size_t num_elem, const u8 *addr[],
			const size_t *len, u8 *mac)
{
	mbedtls_md_context_t ctx;
	size_t i;
	int res = 0;

	mbedtls_md_init(&ctx);
	if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(type), 1) != 0)
		res = -1;

	if (res == 0 && mbedtls_md_hmac_starts(&ctx, key, key_len) != 0)
		res = -1;

	for (i = 0; res == 0 && i < num_elem; i++)
		res = mbedtls_md_hmac_update(&ctx, addr[i], len[i]);

	if (res == 0)
		res = mbedtls_md_hmac_finish(&ctx, mac);
	mbedtls_md_free(&ctx);

	return res == 0 ? 0 : -1;
}

int hmac_sha1_vector(const u8 *key, size_t key_len, size_t num_elem,
		     const u8 *addr[], const size_t *len, u8 *mac)
{
	return mbedtls_hmac_vector(MBEDTLS_MD_SHA1, key, key_len, num_elem, addr,
				len, mac);
}

int hmac_sha1(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	      u8 *mac)
{
	return hmac_sha1_vector(key, key_len, 1, &data, &data_len, mac);
}

int md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	mbedtls_md5_context ctx;
	size_t i;

	mbedtls_md5_init(&ctx);
	mbedtls_md5_starts_ret(&ctx);

	for (i = 0; i < num_elem; ++i)
		mbedtls_md5_update_ret(&ctx, addr[i], len[i]);

	mbedtls_md5_finish_ret(&ctx, mac);
	mbedtls_md5_free(&ctx);

	return 0;
}

int hmac_md5_vector(const u8 *key, size_t key_len, size_t num_elem,
		    const u8 *addr[], const size_t *len, u8 *mac)
{
	return mbedtls_hmac_vector(MBEDTLS_MD_MD5, key, key_len, num_elem, addr,
				len, mac);
}


int hmac_md5(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	     u8 *mac)
{
	return hmac_md5_vector(key, key_len, 1, &data, &data_len, mac);
}

int sha1_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	mbedtls_sha1_context ctx;
	size_t i;

	mbedtls_sha1_init(&ctx);
	mbedtls_sha1_starts_ret(&ctx);

	for (i = 0; i < num_elem; ++i)
		mbedtls_sha1_update_ret(&ctx, addr[i], len[i]);

	mbedtls_sha1_finish_ret(&ctx, mac);
	mbedtls_sha1_free(&ctx);

	return 0;
}

int sha256_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	mbedtls_sha256_context ctx;
	size_t i;

	mbedtls_sha256_init(&ctx);
	mbedtls_sha256_starts_ret(&ctx, 0 /*use SHA256*/);

	for (i = 0; i < num_elem; ++i)
		mbedtls_sha256_update_ret(&ctx, addr[i], len[i]);

	mbedtls_sha256_finish_ret(&ctx, mac);
	mbedtls_sha256_free(&ctx);

	return 0;
}

void * aes_encrypt_init(const u8 *key, size_t len)
{
	mbedtls_aes_context *ctx = os_zalloc(sizeof(*ctx));

	mbedtls_aes_init(ctx);

	if (mbedtls_aes_setkey_enc(ctx, key, len * 8) == 0)
		return ctx;

	os_free(ctx);

	return NULL;
}


int aes_encrypt(void *ctx, const u8 *plain, u8 *crypt)
{
	if (ctx)
		mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_ENCRYPT, plain, crypt);
	else
		return -1;

	return 0;
}


void aes_encrypt_deinit(void *ctx)
{
	if (ctx) {
		mbedtls_aes_free(ctx);
		os_free(ctx);
		ctx = NULL;
	}
}

void *aes_decrypt_init(const u8 *key, size_t len)
{
	mbedtls_aes_context *ctx = os_zalloc(sizeof(*ctx));

	mbedtls_aes_init(ctx);

	if (mbedtls_aes_setkey_dec(ctx, key, len * 8) == 0)
		return ctx;

	os_free(ctx);

	return NULL;
}

int aes_decrypt(void *ctx, const u8 *crypt, u8 *plain)
{
	if (!ctx)
		return -1;

	mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_DECRYPT, crypt, plain);
	return 0;
}

void aes_decrypt_deinit(void *ctx)
{
	if (ctx) {
		mbedtls_aes_free(ctx);
		os_free(ctx);
		ctx = NULL;
	}
}
