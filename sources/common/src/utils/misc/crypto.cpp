/*
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "common.h"

DHWrapper::DHWrapper(const int32_t bitsCount, const uint8_t *pP,
		const size_t sizeP, const uint8_t *pG, const size_t sizeG)
: _bitsCount(bitsCount), _pP(pP), _sizeP(sizeP), _pG(pG), _sizeG(sizeG) {
	_pDH = NULL;
	_pSharedKey = NULL;
	_sharedKeyLength = 0;
}

DHWrapper::~DHWrapper() {
	Cleanup();
}

bool DHWrapper::Initialize() {
	Cleanup();

	//create DH and read p and g
	if (((_pDH = DH_new()) == NULL)
			|| ((_pDH->p = BN_bin2bn(_pP, (int) _sizeP, NULL)) == NULL)
			|| ((_pDH->g = BN_bin2bn(_pG, (int) _sizeG, NULL)) == NULL)) {
		FATAL("Unable to initialize p and g from DH");
		return false;
	}

	//generate the DH
	_pDH->length = _bitsCount;
	if (DH_generate_key(_pDH) != 1) {
		FATAL("Unable to generate DH");
		return false;
	}

	return true;
}

bool DHWrapper::CopyPublicKey(uint8_t *pDst, int32_t dstLength) {
	if (_pDH == NULL) {
		FATAL("DHWrapper not initialized");
		return false;
	}

	return CopyKey(_pDH->pub_key, pDst, dstLength);
}

size_t DHWrapper::GetPublicKeyLength() {
	if ((_pDH == NULL) || (_pDH->pub_key == NULL))
		return 0;
	return BN_num_bytes(_pDH->pub_key);
}

bool DHWrapper::CreateSharedKey(const uint8_t *pPeerPublicKey,
		size_t peerPublicKeySize) {
	if (_pDH == NULL) {
		FATAL("DHWrapper not initialized");
		return false;
	}

	if (_sharedKeyLength != 0 || _pSharedKey != NULL) {
		FATAL("Shared key already computed");
		return false;
	}

	_sharedKeyLength = DH_size(_pDH);
	if (_sharedKeyLength <= 0 || _sharedKeyLength > 1024) {
		FATAL("Unable to get shared key size in bytes");
		return false;
	}
	_pSharedKey = new uint8_t[_sharedKeyLength];
	memset(_pSharedKey, 0, _sharedKeyLength);

	BIGNUM *pPeerPublicKeyNum = BN_bin2bn(pPeerPublicKey, (int) peerPublicKeySize, 0);
	if (pPeerPublicKeyNum == NULL) {
		FATAL("Unable to get the peer public key");
		return false;
	}

	bool result = (DH_compute_key(_pSharedKey, pPeerPublicKeyNum, _pDH) != -1);
	BN_free(pPeerPublicKeyNum);
	pPeerPublicKeyNum = NULL;

	if (!result) {
		FATAL("Unable to compute the shared key");
		return false;
	}

	return true;
}

bool DHWrapper::CopySharedKey(uint8_t *pDst, size_t dstLength) {
	if (_pDH == NULL) {
		FATAL("DHWrapper not initialized");
		return false;
	}

	if (dstLength < _sharedKeyLength) {
		FATAL("Destination is not big enough");
		return false;
	}

	memcpy(pDst, _pSharedKey, _sharedKeyLength);

	return true;
}

size_t DHWrapper::GetSharedKeyLength() {
	if (_pDH == NULL) {
		FATAL("DHWrapper not initialized");
		return 0;
	}
	return _sharedKeyLength;
}

void DHWrapper::Cleanup() {
	if (_pDH != NULL) {
		DH_free(_pDH);
		_pDH = NULL;
	}

	if (_pSharedKey != NULL) {
		delete[] _pSharedKey;
		_pSharedKey = NULL;
	}
	_sharedKeyLength = 0;
}

bool DHWrapper::CopyKey(BIGNUM *pNum, uint8_t *pDst, int32_t dstLength) {
	int32_t keySize = BN_num_bytes(pNum);
	if ((keySize <= 0) || (dstLength <= 0) || (keySize > dstLength)) {
		FATAL("CopyPublicKey failed due to either invalid DH state or invalid call");
		return false;
	}

	if (BN_bn2bin(pNum, pDst) != keySize) {
		FATAL("Unable to copy key");
		return false;
	}

	return true;
}

void InitSSL() {
	//init the random numbers generator
	uint32_t length = 16;
	uint32_t *pBuffer = new uint32_t[length];
	while (RAND_status() == 0) {
		for (uint32_t i = 0; i < length; i++) {
			pBuffer[i] = rand();
		}

		RAND_seed(pBuffer, length * 4);
	}
	delete[] pBuffer;

	//init the SSL library
	SSL_library_init();

	//load SSL resources
	SSL_load_error_strings();
	ERR_load_SSL_strings();
	ERR_load_CRYPTO_strings();
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
}

void InitRC4Encryption(uint8_t *secretKey, uint8_t *pubKeyIn, uint8_t *pubKeyOut,
		RC4_KEY *rc4keyIn, RC4_KEY *rc4keyOut) {
	uint8_t digest[SHA256_DIGEST_LENGTH];
	unsigned int digestLen = 0;

	HMAC_CTX ctx;
	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, secretKey, 128, EVP_sha256(), 0);
	HMAC_Update(&ctx, pubKeyIn, 128);
	HMAC_Final(&ctx, digest, &digestLen);
	HMAC_CTX_cleanup(&ctx);

	RC4_set_key(rc4keyOut, 16, digest);

	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, secretKey, 128, EVP_sha256(), 0);
	HMAC_Update(&ctx, pubKeyOut, 128);
	HMAC_Final(&ctx, digest, &digestLen);
	HMAC_CTX_cleanup(&ctx);

	RC4_set_key(rc4keyIn, 16, digest);
}

string DigestMD5(const string &source, bool textResult) {
	return DigestMD5((uint8_t*) source.data(), (uint32_t) source.length(), textResult);
}

string DigestMD5(uint8_t *pBuffer, uint32_t length, bool textResult) {
	EVP_MD_CTX mdctx;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;

	EVP_DigestInit(&mdctx, EVP_md5());
	EVP_DigestUpdate(&mdctx, pBuffer, length);
	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);

	return textResult ? hex(md_value, md_len) : string((char *) md_value, md_len);
}

bool HMACSHA(const uint8_t *pInputKey, size_t inputKeyLength, uint8_t *pDigest, size_t count, int type, va_list arguments) {
	if ((pInputKey == NULL) || (inputKeyLength == 0))
		return false;
	uint32_t length;
	HMAC_CTX ctx;
	if (type == 1)
		HMAC_Init(&ctx, pInputKey, (int) inputKeyLength, EVP_sha1());
	else if (type == 256)
		HMAC_Init(&ctx, pInputKey, (int) inputKeyLength, EVP_sha256());
	for (size_t i = 0; i < count; i++) {
		const uint8_t *pBuffer = va_arg(arguments, const uint8_t *);
		size_t bufferLength = va_arg(arguments, size_t);
		HMAC_Update(&ctx, pBuffer, bufferLength);
	}
	HMAC_Final(&ctx, pDigest, &length);
	HMAC_cleanup(&ctx);
	return true;
}

bool DigestHMACSHA1(const uint8_t *pInputKey, size_t inputKeyLength, uint8_t *pDigest, size_t count, ...) {
	va_list arguments;
	va_start(arguments, count);
	bool result = HMACSHA(pInputKey, inputKeyLength, pDigest, count, 1, arguments);
	va_end(arguments);
	return result;
}

bool DigestHMACSHA256(const uint8_t *pInputKey, size_t inputKeyLength,
		uint8_t *pDigest, size_t count, ...) {
	va_list arguments;
	va_start(arguments, count);
	bool result = HMACSHA(pInputKey, inputKeyLength, pDigest, count, 256, arguments);
	va_end(arguments);
	return result;
}

static const uint32_t kCrc32Polynomial = 0xEDB88320;
static uint32_t kCrc32Table[256] = {0};
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))

static void EnsureCrc32TableInited() {
	if (kCrc32Table[ARRAY_SIZE(kCrc32Table) - 1])
		return;
	for (uint32_t i = 0; i < ARRAY_SIZE(kCrc32Table); ++i) {
		uint32_t c = i;
		for (size_t j = 0; j < 8; ++j) {
			if (c & 1) {
				c = kCrc32Polynomial ^ (c >> 1);
			} else {
				c >>= 1;
			}
		}
		kCrc32Table[i] = c;
	}
}

uint32_t DigestCRC32Update(uint32_t start, const uint8_t* pBuffer, size_t length) {
	EnsureCrc32TableInited();

	uint32_t c = start ^ 0xFFFFFFFF;
	for (size_t i = 0; i < length; ++i) {
		c = kCrc32Table[(c ^ pBuffer[i]) & 0xFF] ^ (c >> 8);
	}
	return c ^ 0xFFFFFFFF;
}

uint32_t DigestCRC32String(const std::string &src) {
	return DigestCRC32Update(0, (const uint8_t *) src.c_str(), src.length());
}

string b64(const string &source) {
	return b64((uint8_t *) source.data(), (uint32_t) source.size());
}

string b64(uint8_t *pBuffer, uint32_t length) {
	BIO *bmem;
	BIO *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());

	b64 = BIO_push(b64, bmem);
	BIO_write(b64, pBuffer, length);
	string result = "";
	if (BIO_flush(b64) == 1) {
		BIO_get_mem_ptr(b64, &bptr);
		result = string(bptr->data, bptr->length);
	}

	BIO_free_all(b64);


	replace(result, "\n", "");
	replace(result, "\r", "");

	return result;
}

string unb64(const string &source) {
	return unb64((uint8_t *) source.data(), (uint32_t) source.length());
}

string unb64(uint8_t *pBuffer, uint32_t length) {
	// create a memory buffer containing base64 encoded data
	BIO* bmem = BIO_new_mem_buf((void *) pBuffer, length);

	// push a Base64 filter so that reading from buffer decodes it
	BIO *bioCmd = BIO_new(BIO_f_base64());
	// we don't want newlines
	BIO_set_flags(bioCmd, BIO_FLAGS_BASE64_NO_NL);
	bmem = BIO_push(bioCmd, bmem);

	char *pOut = new char[length];

	int finalLen = BIO_read(bmem, (void*) pOut, length);
	BIO_free_all(bmem);
	string result(pOut, finalLen);
	delete[] pOut;
	return result;
}

string hex(const string &source) {
	if (source == "")
		return "";
	return hex((uint8_t *) source.data(), (uint32_t) source.length());
}

string hex(const uint8_t *pBuffer, uint32_t length) {
	if ((pBuffer == NULL) || (length == 0))
		return "";
	string result = "";
	for (uint32_t i = 0; i < length; i++) {
		result += format("%02"PRIx8, pBuffer[i]);
	}
	return result;
}

string bits(const string &source) {
	if (source == "")
		return "";
	return bits((uint8_t *) source.data(), (uint32_t) source.length());
}

string bits(const uint8_t *pBuffer, uint32_t length) {
	string result = "";
	for (uint32_t i = 0; i < length; i++) {
		for (int8_t j = 7; j >= 0; j--) {
			result += ((pBuffer[i] >> j)&0x01) == 1 ? "1" : "0";
		}
	}
	return result;
}

string unhex(const string &source) {
	if (source == "")
		return "";
	if ((source.length() % 2) != 0) {
		FATAL("Invalid hex string: %s", STR(source));
		return "";
	}
	return unhex((uint8_t *) source.data(), (uint32_t) source.length());
}

string unhex(const uint8_t *pBuffer, uint32_t length) {
#define HTOI(x) {\
	if ((x) >= '0' && (x) <= '9') \
		val = (val << 4) + ((x) - '0'); \
	else if ((x) >= 'A' && (x) <= 'F') \
		val = (val << 4) + ((x) - 'A' + 10); \
	else if ((x) >= 'a' && (x) <= 'f') \
		val = (val << 4) + ((x) - 'a' + 10); \
	else { \
		FATAL("Invalid character detected: %c",x); \
		return ""; \
	} \
}
	if ((pBuffer == NULL) || (length == 0) || ((length % 2) != 0))
		return "";
	string result = "";
	char val = 0;
	for (uint32_t i = 0; i < length; i += 2) {
		HTOI(pBuffer[i]);
		HTOI(pBuffer[i + 1]);
		result += val;
	}
	return result;
}

string urlDecode(const string &source) {
	return urlDecode((const uint8_t *) source.data(), source.length());
}

string urlDecode(const uint8_t *pBuffer, size_t length) {
	string result;
	size_t before;
	for (size_t i = 0; i < length;) {
		if (pBuffer[i] == '%') {
			if ((i + 3) > length) {
				FATAL("Invalid input for url decode: `%s`", string((const char *) pBuffer, length).c_str());
				return "";
			}
			before = result.size();
			result += unhex(pBuffer + i + 1, 2);
			if (before == result.size()) {
				FATAL("Invalid input for url decode: `%s`", string((const char *) pBuffer, length).c_str());
				return "";
			}
			i += 3;
		} else {
			result += pBuffer[i];
			i++;
		}
	}
	return result;
}

void CleanupSSL() {
#ifndef NO_SSL_ENGINE_CLEANUP
	ERR_remove_state(0);
	ENGINE_cleanup();
	CONF_modules_unload(1);
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	//fucked up SSL refuses to fix their shit.... I need to get away from OpenSSL fucked up lib...
	STACK_OF(SSL_COMP) *pMethods = SSL_COMP_get_compression_methods();
	if (pMethods != NULL)
		sk_SSL_COMP_free(pMethods);
#endif /* NO_SSL_ENGINE_CLEANUP */
}
