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

#ifdef HAS_PROTOCOL_RTMP

#include "protocols/rtmp/outboundrtmpprotocol.h"
#include "protocols/protocolfactorymanager.h"
#include "netio/netio.h"
#include "protocols/rtmp/rtmpeprotocol.h"
#include "protocols/rtmp/basertmpappprotocolhandler.h"
#include "application/clientapplicationmanager.h"

//#define DEBUG_HANDSHAKE(...) do{printf("%6d - ",__LINE__);printf(__VA_ARGS__);printf("\n");} while(0)
#define DEBUG_HANDSHAKE(...)

OutboundRTMPProtocol::OutboundRTMPProtocol()
: BaseRTMPProtocol(PT_OUTBOUND_RTMP) {
	_pClientPublicKey = NULL;
	_pOutputBuffer = NULL;
	_pClientDigest = NULL;
	_pKeyIn = NULL;
	_pKeyOut = NULL;
	_pDHWrapper = NULL;
	_usedScheme = 0;
	_encrypted = false;
}

OutboundRTMPProtocol::~OutboundRTMPProtocol() {
	if (_pKeyIn != NULL) {
		delete _pKeyIn;
		_pKeyIn = NULL;
	}

	if (_pKeyOut != NULL) {
		delete _pKeyOut;
		_pKeyOut = NULL;
	}

	if (_pDHWrapper != NULL) {
		delete _pDHWrapper;
		_pDHWrapper = NULL;
	}

	if (_pClientPublicKey != NULL) {
		delete[] _pClientPublicKey;
		_pClientPublicKey = NULL;
	}
	if (_pOutputBuffer != NULL) {
		delete[] _pOutputBuffer;
		_pOutputBuffer = NULL;
	}
	if (_pClientDigest != NULL) {
		delete[] _pClientDigest;
		_pClientDigest = NULL;
	}
}

bool OutboundRTMPProtocol::PerformHandshake(IOBuffer &buffer) {
	switch (_rtmpState) {
		case RTMP_STATE_NOT_INITIALIZED:
		{
			_encrypted = (VariantType) _customParameters[CONF_PROTOCOL] == V_STRING &&
					_customParameters[CONF_PROTOCOL] == CONF_PROTOCOL_OUTBOUND_RTMPE;
			_usedScheme = _encrypted ? 1 : 0;

			if ((VariantType) _customParameters[CONF_PROTOCOL] == V_STRING &&
					_customParameters[CONF_PROTOCOL] == CONF_PROTOCOL_OUTBOUND_RTMPE) {
				return PerformHandshakeStage1(true);
			} else {
				return PerformHandshakeStage1(false);
			}
		}
		case RTMP_STATE_CLIENT_REQUEST_SENT:
		{
			if (GETAVAILABLEBYTESCOUNT(buffer) < 3073)
				return true;

			if (!PerformHandshakeStage2(buffer, _encrypted)) {
				FATAL("Unable to handshake");
				return false;
			}

			if (_pFarProtocol != NULL) {
				if (!_pFarProtocol->EnqueueForOutbound()) {
					FATAL("Unable to signal output data");
					return false;
				}
			}

			if (_pKeyIn != NULL && _pKeyOut != NULL) {
				//insert the RTMPE protocol in the current protocol stack
				BaseProtocol *pFarProtocol = GetFarProtocol();
				RTMPEProtocol *pRTMPE = new RTMPEProtocol(_pKeyIn, _pKeyOut,
						GETAVAILABLEBYTESCOUNT(_outputBuffer));
				ResetFarProtocol();
				pFarProtocol->SetNearProtocol(pRTMPE);
				pRTMPE->SetNearProtocol(this);
				//FINEST("New protocol chain: %s", STR(*pFarProtocol));
			}

			if (!buffer.Ignore(3073)) {
				FATAL("Unable to ignore 3073 bytes");
				return false;
			}
			_handshakeCompleted = true;
			return true;
		}
		default:
		{
			FATAL("Invalid RTMP state: %d", _rtmpState);
			return false;
		}
	}
}

bool OutboundRTMPProtocol::Connect(string ip, uint16_t port,
		Variant customParameters) {

	vector<uint64_t> chain = ProtocolFactoryManager::ResolveProtocolChain(
			customParameters[CONF_PROTOCOL]);
	if (chain.size() == 0) {
		FATAL("Unable to obtain protocol chain from settings: %s",
				STR(customParameters[CONF_PROTOCOL]));
		return false;
	}
	if (!TCPConnector<OutboundRTMPProtocol>::Connect(ip, port, chain,
			customParameters)) {
		FATAL("Unable to connect to %s:%hu", STR(ip), port);
		return false;
	}
	return true;
}

bool OutboundRTMPProtocol::SignalProtocolCreated(BaseProtocol *pProtocol,
		Variant customParameters) {
	//1. Get the application  designated for the newly created connection
	if (customParameters[CONF_APPLICATION_NAME] != V_STRING) {
		FATAL("connect parameters must have an application name");
		return false;
	}
	BaseClientApplication *pApplication = ClientApplicationManager::FindAppByName(
			customParameters[CONF_APPLICATION_NAME]);
	if (pApplication == NULL) {
		FATAL("Application %s not found", STR(customParameters[CONF_APPLICATION_NAME]));
		return false;
	}

	if (pProtocol == NULL) {
		FATAL("Connection failed:\n%s", STR(customParameters.ToString()));
		return pApplication->OutboundConnectionFailed(customParameters);
	}

	//2. Set the application
	pProtocol->SetApplication(pApplication);


	//3. Trigger processing, including handshake
	OutboundRTMPProtocol *pOutboundRTMPProtocol = (OutboundRTMPProtocol *) pProtocol;
	pOutboundRTMPProtocol->SetOutboundConnectParameters(customParameters);
	IOBuffer dummy;
	return pOutboundRTMPProtocol->SignalInputData(dummy);
}

bool OutboundRTMPProtocol::PerformHandshakeStage1(bool encrypted) {
	_outputBuffer.ReadFromByte(encrypted ? 6 : 3);

	if (_pOutputBuffer == NULL) {
		_pOutputBuffer = new uint8_t[1536];
	} else {
		delete[] _pOutputBuffer;
		_pOutputBuffer = new uint8_t[1536];
	}

	for (uint32_t i = 0; i < 1536; i++) {
		_pOutputBuffer[i] = rand() % 256;
	}

	EHTONLP(_pOutputBuffer, 0);

	_pOutputBuffer[4] = 9;
	_pOutputBuffer[5] = 0;
	_pOutputBuffer[6] = 124;
	_pOutputBuffer[7] = 2;

	uint32_t clientDHOffset = GetDHOffset(_pOutputBuffer, _usedScheme);

	_pDHWrapper = new DHWrapper(1024);
	if (!_pDHWrapper->Initialize()) {
		FATAL("Unable to initialize DH wrapper");
		return false;
	}

	if (!_pDHWrapper->CopyPublicKey(_pOutputBuffer + clientDHOffset, 128)) {
		FATAL("Couldn't write public key!");
		return false;
	}
	DEBUG_HANDSHAKE("CLIENT: 1. clientDHOffset: %"PRIu32"; _usedScheme: %"PRIu8"; clientPublicKey: %s",
			clientDHOffset,
			_usedScheme,
			STR(hex(_pOutputBuffer + clientDHOffset, 128)));
	_pClientPublicKey = new uint8_t[128];
	memcpy(_pClientPublicKey, _pOutputBuffer + clientDHOffset, 128);


	uint32_t clientDigestOffset = GetDigestOffset(_pOutputBuffer, _usedScheme);

	uint8_t *pTempBuffer = new uint8_t[1536 - 32];
	memcpy(pTempBuffer, _pOutputBuffer, clientDigestOffset);
	memcpy(pTempBuffer + clientDigestOffset, _pOutputBuffer + clientDigestOffset + 32,
			1536 - clientDigestOffset - 32);

	uint8_t *pTempHash = new uint8_t[512];
	HMACsha256(pTempBuffer, 1536 - 32, genuineFPKey, 30, pTempHash);

	memcpy(_pOutputBuffer + clientDigestOffset, pTempHash, 32);
	DEBUG_HANDSHAKE("CLIENT: 2. clientDigestOffset: %"PRIu32"; _usedScheme: %"PRIu8"; clientDigest: %s",
			clientDigestOffset, _usedScheme,
			STR(hex(_pOutputBuffer + clientDigestOffset, 32)));
	_pClientDigest = new uint8_t[32];
	memcpy(_pClientDigest, pTempHash, 32);


	delete[] pTempBuffer;
	delete[] pTempHash;

	_outputBuffer.ReadFromBuffer(_pOutputBuffer, 1536);

	delete[] _pOutputBuffer;
	_pOutputBuffer = NULL;

	if (_pFarProtocol != NULL) {
		if (!_pFarProtocol->EnqueueForOutbound()) {
			FATAL("Unable to signal output data");
			return false;
		}
	}

	_rtmpState = RTMP_STATE_CLIENT_REQUEST_SENT;

	return true;
}

bool OutboundRTMPProtocol::VerifyServer(IOBuffer & inputBuffer) {
	uint8_t *pBuffer = GETIBPOINTER(inputBuffer) + 1;

	uint32_t serverDigestOffset = GetDigestOffset(pBuffer, _usedScheme);
	DEBUG_HANDSHAKE("CLIENT: Validate: 1. serverDigestOffset: %"PRIu32"; _usedScheme: %"PRIu8,
			serverDigestOffset, _usedScheme);

	uint8_t *pTempBuffer = new uint8_t[1536 - 32];
	memcpy(pTempBuffer, pBuffer, serverDigestOffset);
	memcpy(pTempBuffer + serverDigestOffset, pBuffer + serverDigestOffset + 32,
			1536 - serverDigestOffset - 32);

	uint8_t * pDigest = new uint8_t[512];
	HMACsha256(pTempBuffer, 1536 - 32, genuineFMSKey, 36, pDigest);
	DEBUG_HANDSHAKE("CLIENT: Validate: 2. computed serverDigest %s", STR(hex(pDigest, 32)));
	DEBUG_HANDSHAKE("CLIENT: Validate: 3.    found serverDigest %s", STR(hex(pBuffer + serverDigestOffset, 32)));

	int result = memcmp(pDigest, pBuffer + serverDigestOffset, 32);

	delete[] pTempBuffer;
	delete[] pDigest;

	if (result != 0) {
		FATAL("Server not verified");
		return false;
	}

	pBuffer = pBuffer + 1536;

	uint8_t * pChallange = new uint8_t[512];
	HMACsha256(_pClientDigest, 32, genuineFMSKey, 68, pChallange);

	pDigest = new uint8_t[512];
	HMACsha256(pBuffer, 1536 - 32, pChallange, 32, pDigest);

	DEBUG_HANDSHAKE("CLIENT: Validate: 4. computed serverChallange: %s", STR(hex(pDigest, 32)));
	DEBUG_HANDSHAKE("CLIENT: Validate: 5.    found serverChallange: %s", STR(hex(pBuffer + 1536 - 32, 32)));
	result = memcmp(pDigest, pBuffer + 1536 - 32, 32);

	delete[] pChallange;
	delete[] pDigest;

	if (result != 0) {
		FATAL("Server not verified");
		return false;
	}

	return true;
}

bool OutboundRTMPProtocol::PerformHandshakeStage2(IOBuffer &inputBuffer,
		bool encrypted) {
	if (encrypted || _pProtocolHandler->ValidateHandshake()) {
		if (!VerifyServer(inputBuffer)) {
			FATAL("Unable to verify server");
			return false;
		}
	}

	uint8_t *pInputBuffer = GETIBPOINTER(inputBuffer) + 1;

	uint32_t serverDHOffset = GetDHOffset(pInputBuffer, _usedScheme);
	if (_pDHWrapper == NULL) {
		FATAL("dh wrapper not initialized");
		return false;
	}

	DEBUG_HANDSHAKE("CLIENT: 1. serverDHOffset: %"PRIu32"; _usedScheme: %"PRIu8"; serverPublicKey: %s",
			serverDHOffset,
			_usedScheme,
			STR(hex(pInputBuffer + serverDHOffset, 128)));
	if (!_pDHWrapper->CreateSharedKey(pInputBuffer + serverDHOffset, 128)) {
		FATAL("Unable to create shared key");
		return false;
	}

	uint8_t secretKey[128];
	if (!_pDHWrapper->CopySharedKey(secretKey, sizeof (secretKey))) {
		FATAL("Unable to compute shared");
		return false;
	}
	DEBUG_HANDSHAKE("CLIENT: 2. secretKey: %s", STR(hex(secretKey, 128)));

	if (encrypted) {
		_pKeyIn = new RC4_KEY;
		_pKeyOut = new RC4_KEY;

		InitRC4Encryption(
				secretKey,
				(uint8_t*) & pInputBuffer[serverDHOffset],
				_pClientPublicKey,
				_pKeyIn,
				_pKeyOut);

		uint8_t data[1536];
		RC4(_pKeyIn, 1536, data, data);
		RC4(_pKeyOut, 1536, data, data);
	}

	delete _pDHWrapper;
	_pDHWrapper = NULL;

	uint32_t serverDigestOffset = GetDigestOffset(pInputBuffer, _usedScheme);
	DEBUG_HANDSHAKE("CLIENT: 3. serverDigestOffset: %"PRIu32"; _usedScheme: %"PRIu8, serverDigestOffset, _usedScheme);

	if (_pOutputBuffer == NULL) {
		_pOutputBuffer = new uint8_t[1536];
	} else {
		delete[] _pOutputBuffer;
		_pOutputBuffer = new uint8_t[1536];
	}

	for (uint32_t i = 0; i < 1536; i++) {
		_pOutputBuffer[i] = rand() % 256;
	}

	uint8_t * pChallangeKey = new uint8_t[512];
	HMACsha256(pInputBuffer + serverDigestOffset, 32, genuineFPKey, 62, pChallangeKey);

	uint8_t * pDigest = new uint8_t[512];
	HMACsha256(_pOutputBuffer, 1536 - 32, pChallangeKey, 32, pDigest);

	memcpy(_pOutputBuffer + 1536 - 32, pDigest, 32);
	DEBUG_HANDSHAKE("CLIENT: 4. clientChallange: %s", STR(hex(pDigest, 32)));

	delete[] pChallangeKey;
	delete[] pDigest;

	_outputBuffer.ReadFromBuffer(_pOutputBuffer, 1536);

	delete[] _pOutputBuffer;
	_pOutputBuffer = NULL;

	_rtmpState = RTMP_STATE_DONE;

	return true;
}
#endif /* HAS_PROTOCOL_RTMP */

