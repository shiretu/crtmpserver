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


#include "utils/misc/crypto.h"
#include "utils/misc/dhgroups.h"
#include "utils/logging/logging.h"

DHWrapper::DHWrapper(int32_t bitsCount) {
	_bitsCount = bitsCount;
	_pDH = NULL;
	_pSharedKey = NULL;
	_sharedKeyLength = 0;
	_peerPublickey = NULL;
}

DHWrapper::~DHWrapper() {
	Cleanup();
}

bool DHWrapper::Initialize() {
	Cleanup();

	//1. Create the DH
	_pDH = DH_new();
	if (_pDH == NULL) {
		FATAL("Unable to create DH");
		Cleanup();
		return false;
	}

	//2. Create his internal p and g
	_pDH->p = BN_new();
	if (_pDH->p == NULL) {
		FATAL("Unable to create p");
		Cleanup();
		return false;
	}
	_pDH->g = BN_new();
	if (_pDH->g == NULL) {
		FATAL("Unable to create g");
		Cleanup();
		return false;
	}

	//3. initialize p, g and key length
	if (BN_hex2bn(&_pDH->p, P1024) == 0) {
		FATAL("Unable to parse P1024");
		Cleanup();
		return false;
	}
	if (BN_set_word(_pDH->g, 2) != 1) {
		FATAL("Unable to set g");
		Cleanup();
		return false;
	}

	//4. Set the key length
	_pDH->length = _bitsCount;

	//5. Generate private and public key
	if (DH_generate_key(_pDH) != 1) {
		FATAL("Unable to generate DH public/private keys");
		Cleanup();
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

bool DHWrapper::CopyPrivateKey(uint8_t *pDst, int32_t dstLength) {
	if (_pDH == NULL) {
		FATAL("DHWrapper not initialized");
		return false;
	}

	return CopyKey(_pDH->priv_key, pDst, dstLength);
}

bool DHWrapper::CreateSharedKey(uint8_t *pPeerPublicKey, int32_t length) {
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

	_peerPublickey = BN_bin2bn(pPeerPublicKey, length, 0);
	if (_peerPublickey == NULL) {
		FATAL("Unable to get the peer public key");
		return false;
	}

	if (DH_compute_key(_pSharedKey, _peerPublickey, _pDH) == -1) {
		FATAL("Unable to compute the shared key");
		return false;
	}

	return true;
}

bool DHWrapper::CopySharedKey(uint8_t *pDst, int32_t dstLength) {
	if (_pDH == NULL) {
		FATAL("DHWrapper not initialized");
		return false;
	}

	if (dstLength != _sharedKeyLength) {
		FATAL("Destination has different size");
		return false;
	}

	memcpy(pDst, _pSharedKey, _sharedKeyLength);

	return true;
}

void DHWrapper::Cleanup() {
	if (_pDH != NULL) {
		if (_pDH->p != NULL) {
			BN_free(_pDH->p);
			_pDH->p = NULL;
		}
		if (_pDH->g != NULL) {
			BN_free(_pDH->g);
			_pDH->g = NULL;
		}
		DH_free(_pDH);
		_pDH = NULL;
	}

	if (_pSharedKey != NULL) {
		delete[] _pSharedKey;
		_pSharedKey = NULL;
	}
	_sharedKeyLength = 0;

	if (_peerPublickey != NULL) {
		BN_free(_peerPublickey);
		_peerPublickey = NULL;
	}
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

string md5(string source, bool textResult) {
	return md5((uint8_t*) source.data(), (uint32_t) source.length(), textResult);
}

string md5(uint8_t *pBuffer, uint32_t length, bool textResult) {
	EVP_MD_CTX mdctx;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;

	EVP_DigestInit(&mdctx, EVP_md5());
	EVP_DigestUpdate(&mdctx, pBuffer, length);
	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);

	if (textResult) {
		string result = "";
		for (uint32_t i = 0; i < md_len; i++) {
			result += format("%02hhx", md_value[i]);
		}
		return result;
	} else {
		return string((char *) md_value, md_len);
	}
}

string sha256(string source) {
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, source.c_str(), source.size());
	SHA256_Final(hash, &sha256);
	int i = 0;
	char outputBuffer[65];
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
	}
	outputBuffer[64] = 0;
	return format("%s", outputBuffer);
}

void HMACsha256(const void *pData, uint32_t dataLength,
		const void *pKey, uint32_t keyLength, void *pResult) {
	unsigned int digestLen;

	HMAC_CTX ctx;
	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, (unsigned char*) pKey, keyLength, EVP_sha256(), NULL);
	HMAC_Update(&ctx, (unsigned char *) pData, dataLength);
	HMAC_Final(&ctx, (unsigned char *) pResult, &digestLen);
	HMAC_CTX_cleanup(&ctx);

	o_assert(digestLen == 32);
}

string b64(string source) {
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

string unb64(string source) {
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

string hex(string source) {
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

string bits(string source) {
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

string unhex(string source) {
	if (source == "")
		return "";
	if ((source.length() % 2) != 0) {
		FATAL("Invalid hex string: %s", STR(source));
		return "";
	}
	return unhex((uint8_t *) source.data(), (uint32_t) source.length());
}

string unhex(const uint8_t *pBuffer, uint32_t length) {
	if ((pBuffer == NULL) || (length == 0) || ((length % 2) != 0))
		return "";
	string result = "";
	uint32_t index = 0;
	for (uint32_t i = 0; i < (length / 2); i++) {
		uint8_t val = 0;
		index = i * 2;
		if ((pBuffer[index] >= '0') && (pBuffer[index] <= '9')) {
			val = (pBuffer[index] - '0') << 4;
		} else if ((pBuffer[index] >= 'a') && (pBuffer[index] <= 'f')) {
			val = (pBuffer[index] - 'a' + 10) << 4;
		} else if ((pBuffer[index] >= 'A') && (pBuffer[index] <= 'F')) {
			val = (pBuffer[index] - 'A' + 10) << 4;
		} else {
			FATAL("Invalid hex string");
			return "";
		}
		if ((pBuffer[index + 1] >= '0') && (pBuffer[index + 1] <= '9')) {
			val |= (pBuffer[index + 1] - '0');
		} else if ((pBuffer[index + 1] >= 'a') && (pBuffer[index + 1] <= 'f')) {
			val |= (pBuffer[index + 1] - 'a' + 10);
		} else if ((pBuffer[index + 1] >= 'A') && (pBuffer[index + 1] <= 'F')) {
			val |= (pBuffer[index + 1] - 'A' + 10);
		} else {
			FATAL("Invalid hex string");
			return "";
		}
		result += (char) val;
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
#endif /* NO_SSL_ENGINE_CLEANUP */
}
