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

#include "protocols/ssl/outboundsslprotocol.h"

OutboundSSLProtocol::OutboundSSLProtocol()
: BaseSSLProtocol(PT_OUTBOUND_SSL) {

}

OutboundSSLProtocol::~OutboundSSLProtocol() {
}

bool OutboundSSLProtocol::InitGlobalContext(Variant &parameters) {
	//1. get the hash which is always the same for client connetions
	string hash = "clientConnection";
	_pGlobalSSLContext = _pGlobalContexts[hash];
	if (_pGlobalSSLContext == NULL) {
		//2. prepare the global ssl context
		_pGlobalSSLContext = SSL_CTX_new(TLSv1_method());
		if (_pGlobalSSLContext == NULL) {
			FATAL("Unable to create global SSL context");
			return false;
		}

		//3. Set verify location for server certificate authentication, if any
		if (parameters.HasKeyChain(V_STRING, false, 1, "serverCert")) {
			string serverCertData = (string) parameters.GetValue("serverCert", true);
			X509_STORE * x509Ctx = SSL_CTX_get_cert_store(_pGlobalSSLContext);
			if ((x509Ctx != NULL)&&(serverCertData.size() != 0)) {
				BIO * bio = BIO_new(BIO_s_mem());
				BIO_write(bio, (const void *) serverCertData.data(),
						(int) serverCertData.length());
				X509 * x509ServerCert = d2i_X509_bio(bio, NULL);
				if (X509_STORE_add_cert(x509Ctx, x509ServerCert) == 0) {
					FATAL("Unable to load Server CA. Error(s): %s", STR(GetSSLErrors()));
					return false;
				}
				SSL_CTX_set_verify(_pGlobalSSLContext, SSL_VERIFY_PEER, NULL);
			}
		} else if (parameters.HasKeyChain(V_MAP, false, 1, "serverCerts")) {
			Variant certs = parameters["serverCerts"];
			X509_STORE * x509Ctx = SSL_CTX_get_cert_store(_pGlobalSSLContext);

			if (certs.IsArray() && x509Ctx != NULL) {
				for (uint32_t i = 0; i < certs.MapSize(); i++) {
					string servCert = certs[i];
					if (servCert.size() != 0) {
						BIO * bio = BIO_new(BIO_s_mem());
						BIO_write(bio, (const void *) servCert.data(),
								(int) servCert.length());
						X509 * x509ServerCert = d2i_X509_bio(bio, NULL);
						if (X509_STORE_add_cert(x509Ctx, x509ServerCert) == 0) {
							FATAL("Unable to load Server CA. Error(s): %s", STR(GetSSLErrors()));
						}
					}
				}
			}
		}
		
		string key;
		string cert;
		if ((parameters.HasKeyChain(V_STRING, false, 1, CONF_SSL_KEY))
				&&(parameters.HasKeyChain(V_STRING, false, 1, CONF_SSL_CERT))) {
			key = (string) parameters.GetValue(CONF_SSL_KEY, false);
			cert = (string) parameters.GetValue(CONF_SSL_CERT, false);
		}

		//4. Load client key and certificate, if any
		if ((key != "")&&(cert != "")) {
			//5. Setup certificate from string
			if (SSL_CTX_use_certificate_ASN1(_pGlobalSSLContext, (int) cert.size(),
					(unsigned char *) cert.data()) <= 0) {
				FATAL("Unable to load certificate %s; Error(s) was: %s",
						STR(cert),
						STR(GetSSLErrors()));
				SSL_CTX_free(_pGlobalSSLContext);
				_pGlobalSSLContext = NULL;
				return false;
			}

			//6. Setup private key
			if (SSL_CTX_use_RSAPrivateKey_ASN1(_pGlobalSSLContext,
					(unsigned char *) key.data(), (long) key.size()) <= 0) {
				FATAL("Unable to load key %s; Error(s) was: %s",
						STR(key),
						STR(GetSSLErrors()));
				SSL_CTX_free(_pGlobalSSLContext);
				_pGlobalSSLContext = NULL;
				return false;
			}

		}
		//7. Store the global context for later usage
		_pGlobalContexts[hash] = _pGlobalSSLContext;
	}
	return true;
}

bool OutboundSSLProtocol::DoHandshake() {
	if (_sslHandshakeCompleted)
		return true;
	int32_t errorCode = SSL_ERROR_NONE;
	errorCode = SSL_connect(_pSSL);
	if (errorCode < 0) {
		int32_t error = SSL_get_error(_pSSL, errorCode);
		if (error != SSL_ERROR_WANT_READ &&
				error != SSL_ERROR_WANT_WRITE) {
			FATAL("Unable to connect SSL: %d; %s", error, STR(GetSSLErrors()));
			return false;
		}
	}

	_sslHandshakeCompleted = SSL_is_init_finished(_pSSL);

	if (!PerformIO()) {
		FATAL("Unable to perform I/O");
		return false;
	}

	if (_sslHandshakeCompleted)
		return EnqueueForOutbound();

	return true;
}
