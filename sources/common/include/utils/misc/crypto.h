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


#pragma once

#include "platform/platform.h"
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/rc4.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>
#include <openssl/engine.h>
#include <openssl/conf.h>

/*!
	@class DHWrapper
	@brief Class that handles the DH wrapper
 */
class DLLEXP DHWrapper {
private:
	const int32_t _bitsCount;
	const uint8_t *_pP;
	const size_t _sizeP;
	const uint8_t *_pG;
	const size_t _sizeG;
	DH *_pDH;
	uint8_t *_pSharedKey;
	size_t _sharedKeyLength;
public:
	DHWrapper(const int32_t bitsCount, const uint8_t *pP, const size_t sizeP,
			const uint8_t *pG, const size_t sizeG);
	virtual ~DHWrapper();

	/*!
		@brief Initializes the DH wrapper
	 */
	bool Initialize();

	/*!
		@brief Copies the public key.
		@param pDst - Where the copied key is stored
		@param dstLength
	 */
	bool CopyPublicKey(uint8_t *pDst, int32_t dstLength);

	/*!
		@brief returns the public key length in bytes
	 */
	size_t GetPublicKeyLength();

	/*!
		@brief Creates a shared secret key
		@param pPeerPublicKey
		@param length
	 */
	bool CreateSharedKey(const uint8_t *pPeerPublicKey, size_t peerPublicKeySize);

	/*!
		@brief Copies the shared secret key.
		@param pDst - Where the copied key is stored
		@param dstLength
	 */
	bool CopySharedKey(uint8_t *pDst, size_t dstLength);

	/*!
		@brief returns the shared key length in bytes
	 */
	size_t GetSharedKeyLength();
private:
	void Cleanup();
	bool CopyKey(BIGNUM *pNum, uint8_t *pDst, int32_t dstLength);
};

DLLEXP void InitSSL();
DLLEXP void InitRC4Encryption(uint8_t *secretKey, uint8_t *pubKeyIn, uint8_t *pubKeyOut,
		RC4_KEY *rc4keyIn, RC4_KEY *rc4keyOut);
DLLEXP string DigestMD5(const string &source, bool textResult);
DLLEXP string DigestMD5(uint8_t *pBuffer, uint32_t length, bool textResult);
DLLEXP bool DigestHMACSHA1(const uint8_t *pInputKey, size_t inputKeyLength,
		uint8_t *pDigest, size_t count, ...);
DLLEXP bool DigestHMACSHA256(const uint8_t *pInputKey, size_t inputKeyLength,
		uint8_t *pDigest, size_t count, ...);
DLLEXP uint32_t DigestCRC32Update(uint32_t start, const uint8_t* pBuffer, size_t length);
DLLEXP uint32_t DigestCRC32String(const std::string &src);
DLLEXP string b64(const string &source);
DLLEXP string b64(uint8_t *pBuffer, uint32_t length);
DLLEXP string unb64(const string &source);
DLLEXP string unb64(uint8_t *pBuffer, uint32_t length);
DLLEXP string hex(const string &source);
DLLEXP string hex(const uint8_t *pBuffer, uint32_t length);
DLLEXP string bits(const string &source);
DLLEXP string bits(const uint8_t *pBuffer, uint32_t length);
DLLEXP string unhex(const string &source);
DLLEXP string unhex(const uint8_t *pBuffer, uint32_t length);
DLLEXP string urlDecode(const string &source);
DLLEXP string urlDecode(const uint8_t *pBuffer, size_t length);
DLLEXP void CleanupSSL();
