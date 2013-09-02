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


#ifdef HAS_PROTOCOL_RTP
#include "protocols/rtp/rtspprotocol.h"
#include "application/baseclientapplication.h"
#include "protocols/rtp/basertspappprotocolhandler.h"
#include "protocols/protocolmanager.h"
#include "streaming/streamstypes.h"
#include "streaming/baseinnetstream.h"
#include "protocols/rtp/streaming/outnetrtpudph264stream.h"
#include "protocols/rtp/streaming/innetrtpstream.h"
#include "protocols/protocolfactorymanager.h"
#include "protocols/rtp/inboundrtpprotocol.h"
#include "netio/netio.h"
#include "protocols/rtp/connectivity/outboundconnectivity.h"
#include "protocols/rtp/connectivity/inboundconnectivity.h"
#include "protocols/protocolmanager.h"
#include "protocols/http/httpauthhelper.h"
#include "protocols/rtmp/basertmpappprotocolhandler.h"
#include "protocols/rtmp/streaming/infilertmpstream.h"
#include "mediaformats/readers/streammetadataresolver.h"
#include "protocols/passthrough/passthroughprotocol.h"

#define RTSP_STATE_HEADERS 0
#define RTSP_STATE_PAYLOAD 1
#define RTSP_MAX_LINE_LENGTH 256
#define RTSP_MAX_HEADERS_COUNT 64
#define RTSP_MAX_HEADERS_SIZE 2048
#define RTSP_MAX_CHUNK_SIZE 1024*128

RTSPProtocol::RTSPKeepAliveTimer::RTSPKeepAliveTimer(uint32_t protocolId)
: BaseTimerProtocol() {
	_protocolId = protocolId;
}

RTSPProtocol::RTSPKeepAliveTimer::~RTSPKeepAliveTimer() {

}

bool RTSPProtocol::RTSPKeepAliveTimer::TimePeriodElapsed() {
	RTSPProtocol *pProtocol = (RTSPProtocol *) ProtocolManager::GetProtocol(_protocolId);
	if (pProtocol == NULL) {
		FATAL("Unable to get parent protocol");
		EnqueueForDelete();
		return true;
	}
	if (!pProtocol->SendKeepAlive()) {
		FATAL("Unable to send keep alive options");
		pProtocol->EnqueueForDelete();
		return true;
	}

	return true;
}

RTSPProtocol::RTSPProtocol()
: BaseProtocol(PT_RTSP) {
	_state = RTSP_STATE_HEADERS;
	_rtpData = false;
	_rtpDataLength = 0;
	_rtpDataChanel = 0;
	_contentLength = 0;
	_requestSequence = 0;
	_pOutboundConnectivity = NULL;
	_pInboundConnectivity = NULL;
	_keepAliveTimerId = 0;
	_pOutStream = NULL;
	_enableTearDown = false;
	_keepAliveMethod = RTSP_METHOD_OPTIONS;
	_maxBufferSize = 256 * 1024;
	_clientSideBuffer = 2;
	_isHTTPTunneled = false;
	_passThroughProtocolId = 0;
}

RTSPProtocol::~RTSPProtocol() {
	CloseOutboundConnectivity();
	CloseInboundConnectivity();
	if (ProtocolManager::GetProtocol(_keepAliveTimerId) != NULL) {
		ProtocolManager::GetProtocol(_keepAliveTimerId)->EnqueueForDelete();
	}
	if (_pOutStream != NULL) {
		delete _pOutStream;
		_pOutStream = NULL;
	}
	BaseProtocol *pProtocol = ProtocolManager::GetProtocol(_passThroughProtocolId);
	if (pProtocol != NULL)
		pProtocol->GracefullyEnqueueForDelete();
}

IOBuffer * RTSPProtocol::GetOutputBuffer() {
	if (GETAVAILABLEBYTESCOUNT(_outputBuffer))
		return &_outputBuffer;
	return NULL;
}

bool RTSPProtocol::Initialize(Variant &parameters) {
	GetCustomParameters() = parameters;
	return true;
}

void RTSPProtocol::SetApplication(BaseClientApplication *pApplication) {
	BaseProtocol::SetApplication(pApplication);
	if (pApplication != NULL) {
		_pProtocolHandler = (BaseRTSPAppProtocolHandler *)
				pApplication->GetProtocolHandler(GetType());
		if (_pProtocolHandler == NULL) {
			FATAL("Protocol handler not found");
			EnqueueForDelete();
		}
		if (pApplication->GetConfiguration().HasKeyChain(_V_NUMERIC, false, 1, "maxRtspOutBuffer")) {
			_maxBufferSize = (uint32_t) pApplication->GetConfiguration().GetValue("maxRtspOutBuffer", false);
		}
	} else {
		_pProtocolHandler = NULL;
	}
}

bool RTSPProtocol::AllowFarProtocol(uint64_t type) {
	return type == PT_TCP;
}

bool RTSPProtocol::AllowNearProtocol(uint64_t type) {
	return type == PT_INBOUND_RTP;
}

bool RTSPProtocol::SignalInputData(int32_t recvAmount) {
	NYIR;
}

void RTSPProtocol::GetStats(Variant &info, uint32_t namespaceId) {
	BaseProtocol::GetStats(info, namespaceId);
	info["streams"].IsArray(true);
	Variant si;
	if (GetApplication() != NULL) {
		StreamsManager *pStreamsManager = GetApplication()->GetStreamsManager();
		map<uint32_t, BaseStream*> streams = pStreamsManager->FindByProtocolId(GetId());

		FOR_MAP(streams, uint32_t, BaseStream *, i) {
			si.Reset();
			MAP_VAL(i)->GetStats(si, namespaceId);
			info["streams"].PushToArray(si);
		}
	}
}

void RTSPProtocol::EnqueueForDelete() {
	if (!_enableTearDown) {
		BaseProtocol::EnqueueForDelete();
		return;
	}
	_enableTearDown = false;
	Variant &params = GetCustomParameters();
	Variant &variantUri = params["uri"];
	string uri = (string) variantUri["fullUri"];
	PushRequestFirstLine(RTSP_METHOD_TEARDOWN, uri, RTSP_VERSION_1_0);
	SendRequestMessage();
	GracefullyEnqueueForDelete();
}

void RTSPProtocol::IsHTTPTunneled(bool value) {
	_isHTTPTunneled = value;
}

bool RTSPProtocol::IsHTTPTunneled() {
	return _isHTTPTunneled;
}

bool RTSPProtocol::OpenHTTPTunnel() {
	Variant &params = GetCustomParameters();
	if (!params.HasKeyChain(V_STRING, true, 2, "uri", "fullUri")) {
		FATAL("URI not found");
		return false;
	}
	_httpTunnelHostPort = format("%s:%"PRIu16, STR(params["uri"]["host"]), (uint16_t) params["uri"]["port"]);
	_httpTunnelUri = format("http://%s%s",
			STR(_httpTunnelHostPort),
			STR(params["uri"]["fullDocumentPathWithParameters"]));
	_xSessionCookie = generateRandomString(22);

	PushRequestFirstLine(HTTP_METHOD_GET, _httpTunnelUri, HTTP_VERSION_1_0);
	PushRequestHeader(RTSP_HEADERS_ACCEPT, "application/x-rtsp-tunnelled");
	PushRequestHeader(HTTP_HEADERS_HOST, _httpTunnelHostPort);
	PushRequestHeader("Pragma", "no-cache");
	PushRequestHeader(HTTP_HEADERS_CACHE_CONTROL, HTTP_HEADERS_CACHE_CONTROL_NO_CACHE);
	Variant &auth = _authentication["rtsp"];
	if (auth == V_MAP) {
		if (!HTTPAuthHelper::GetAuthorizationHeader(
				(string) auth["authenticateHeader"],
				(string) auth["userName"],
				(string) auth["password"],
				_httpTunnelUri,
				HTTP_METHOD_GET,
				auth["temp"])) {
			FATAL("Unable to create authentication header");
			return false;
		}

		PushRequestHeader(HTTP_HEADERS_AUTORIZATION, auth["temp"]["authorizationHeader"]["raw"]);
	}
	return SendRequestMessage();
}

string RTSPProtocol::GetSessionId() {
	return _sessionId;
}

string RTSPProtocol::GenerateSessionId() {
	if (_sessionId != "")
		return _sessionId;
	_sessionId = generateRandomString(8);
	return _sessionId;
}

bool RTSPProtocol::SetSessionId(string sessionId) {
	vector<string> parts;
	split(sessionId, ";", parts);
	if (parts.size() >= 1)
		sessionId = parts[0];
	if (_sessionId != "")
		return _sessionId == sessionId;
	_sessionId = sessionId;
	return true;
}

bool RTSPProtocol::SetAuthentication(string authenticateHeader, string userName,
		string password, bool isRtsp) {
	Variant &auth = isRtsp ? _authentication["rtsp"] : _authentication["proxy"];
	//1. Setup the authentication
	if (auth != V_NULL) {
		FATAL("Authentication was setup but it failed");
		return false;
	}
	auth["userName"] = userName;
	auth["password"] = password;
	auth["authenticateHeader"] = authenticateHeader;

	//2. re-do the request
	return true;
}

bool RTSPProtocol::EnableKeepAlive(uint32_t period, string keepAliveURI) {
	RTSPKeepAliveTimer *pTimer = new RTSPKeepAliveTimer(GetId());
	_keepAliveTimerId = pTimer->GetId();
	_keepAliveURI = keepAliveURI;
	trim(_keepAliveURI);
	if (_keepAliveURI == "")
		_keepAliveURI = "*";
	return pTimer->EnqueueForTimeEvent(period);
}

void RTSPProtocol::EnableTearDown() {
	_enableTearDown = true;
}

void RTSPProtocol::SetKeepAliveMethod(string keepAliveMethod) {
	_keepAliveMethod = keepAliveMethod;
}

bool RTSPProtocol::SendKeepAlive() {
	PushRequestFirstLine(_keepAliveMethod, _keepAliveURI, RTSP_VERSION_1_0);
	if (GetCustomParameters().HasKey(RTSP_HEADERS_SESSION)) {
		PushRequestHeader(RTSP_HEADERS_SESSION,
				GetCustomParameters()[RTSP_HEADERS_SESSION]);
	}
	return SendRequestMessage();
}

bool RTSPProtocol::HasConnectivity() {
	return ((_pInboundConnectivity != NULL) || (_pOutboundConnectivity != NULL));
}

SDP &RTSPProtocol::GetInboundSDP() {
	return _inboundSDP;
}

bool RTSPProtocol::FeedRTSPRequest(string &data) {
	if (data.size() == 0)
		return true;
	_internalBuffer.ReadFromBuffer((const uint8_t *) data.data(), (uint32_t) data.size());
	return SignalInputData(_internalBuffer);
}

bool RTSPProtocol::SignalInputData(IOBuffer &buffer) {
	while (GETAVAILABLEBYTESCOUNT(buffer) > 0) {
		switch (_state) {
			case RTSP_STATE_HEADERS:
			{
				if (!ParseHeaders(buffer)) {
					FATAL("Unable to read headers");
					return false;
				}
				if (_state != RTSP_STATE_PAYLOAD) {
					return true;
				}
			}
			case RTSP_STATE_PAYLOAD:
			{
				if (_rtpData) {
					if (_pInboundConnectivity != NULL) {
						if (!_pInboundConnectivity->FeedData(
								_rtpDataChanel, GETIBPOINTER(buffer), _rtpDataLength)) {
							FATAL("Unable to handle raw RTP packet");
							return false;
						}
					}
					buffer.Ignore(_rtpDataLength);
					_state = RTSP_STATE_HEADERS;
				} else {
					if (!HandleRTSPMessage(buffer)) {
						FATAL("Unable to handle content");
						return false;
					}
				}
				break;
			}
			default:
			{
				ASSERT("Invalid RTSP state");
				return false;
			}
		}
	}

	return true;
}

void RTSPProtocol::PushRequestFirstLine(string method, string url,
		string version) {
	_requestHeaders.Reset();
	_requestContent = "";
	_requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD] = method;
	_requestHeaders[RTSP_FIRST_LINE][RTSP_URL] = url;
	_requestHeaders[RTSP_FIRST_LINE][RTSP_VERSION] = version;
}

void RTSPProtocol::PushRequestHeader(string name, string value) {
	_requestHeaders[RTSP_HEADERS][name] = value;
}

void RTSPProtocol::PushRequestContent(string outboundContent, bool append) {
	if (append)
		_requestContent += "\r\n" + outboundContent;
	else
		_requestContent = outboundContent;
}

bool RTSPProtocol::SendRequestMessage() {
	bool isHttpRequest = (_requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD] == HTTP_METHOD_GET)
			|| (_requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD] == HTTP_METHOD_POST);

	//2. Put our request sequence in place
	if (isHttpRequest) {
		_requestHeaders[RTSP_HEADERS]["x-sessioncookie"] = _xSessionCookie;
		_pendingRequestHeaders[0xffffffff] = _requestHeaders;
		_pendingRequestContent[0xffffffff] = _requestContent;
	} else {
		if (_requestSequence == 0xffffffff) {
			FATAL("Maximum number of requests reached");
			return false;
		}
		_requestHeaders[RTSP_HEADERS][RTSP_HEADERS_CSEQ] = format("%"PRIu32, ++_requestSequence);

		//3. Put authentication if necessary
		Variant &auth = _authentication["rtsp"];
		if (auth == V_MAP) {
			if (!HTTPAuthHelper::GetAuthorizationHeader(
					(string) auth["authenticateHeader"],
					(string) auth["userName"],
					(string) auth["password"],
					(string) _requestHeaders[RTSP_FIRST_LINE][RTSP_URL],
					(string) _requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD],
					auth["temp"])) {
				FATAL("Unable to create authentication header");
				return false;
			}

			_requestHeaders[RTSP_HEADERS][HTTP_HEADERS_AUTORIZATION] =
					auth["temp"]["authorizationHeader"]["raw"];
		}

		_pendingRequestHeaders[_requestSequence] = _requestHeaders;
		_pendingRequestContent[_requestSequence] = _requestContent;
	}

	if (_pendingRequestHeaders.size() > 10 || _pendingRequestContent.size() > 10) {
		FATAL("Requests backlog count too high");
		return false;
	}

	string date;
	char tempBuff[128] = {0};
	struct tm tm;
	time_t t = getutctime();
	gmtime_r(&t, &tm);
	strftime(tempBuff, 127, "%a, %d %b %Y %H:%M:%S UTC", &tm);
	date = tempBuff;

	_requestHeaders[RTSP_HEADERS]["Date"] = date;
	_requestHeaders[RTSP_HEADERS][RTSP_HEADERS_X_POWERED_BY] = RTSP_HEADERS_X_POWERED_BY_US;
	_requestHeaders[RTSP_HEADERS].RemoveKey(RTSP_HEADERS_SERVER, false);

	string firstLine =
			(string) _requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD]
			+ " " + (string) _requestHeaders[RTSP_FIRST_LINE][RTSP_URL]
			+ " " + (string) _requestHeaders[RTSP_FIRST_LINE][RTSP_VERSION];

	//3. send the mesage
	return SendMessage(firstLine, _requestHeaders, _requestContent);
}

uint32_t RTSPProtocol::LastRequestSequence() {
	return _requestSequence;
}

bool RTSPProtocol::GetRequest(uint32_t seqId, Variant &result, string &content) {
	if ((!MAP_HAS1(_pendingRequestHeaders, seqId))
			|| (!MAP_HAS1(_pendingRequestContent, seqId))) {
		MAP_ERASE1(_pendingRequestHeaders, seqId);
		MAP_ERASE1(_pendingRequestContent, seqId);
		return false;
	}
	result = _pendingRequestHeaders[seqId];
	content = _pendingRequestContent[seqId];
	MAP_ERASE1(_pendingRequestHeaders, seqId);
	MAP_ERASE1(_pendingRequestContent, seqId);
	return true;
}

void RTSPProtocol::ClearResponseMessage() {
	_responseHeaders.Reset();
	_responseContent = "";
}

void RTSPProtocol::PushResponseFirstLine(string version, uint32_t code,
		string reason) {
	_responseHeaders[RTSP_FIRST_LINE][RTSP_VERSION] = version;
	_responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE] = code;
	_responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE_REASON] = reason;
}

void RTSPProtocol::PushResponseHeader(string name, string value) {
	_responseHeaders[RTSP_HEADERS][name] = value;
}

void RTSPProtocol::PushResponseContent(string outboundContent, bool append) {
	if (append)
		_responseContent += "\r\n" + outboundContent;
	else
		_responseContent = outboundContent;
}

bool RTSPProtocol::SendResponseMessage() {
	string date;
	char tempBuff[128] = {0};
	struct tm tm;
	time_t t = getutctime();
	gmtime_r(&t, &tm);
	strftime(tempBuff, 127, "%a, %d %b %Y %H:%M:%S UTC", &tm);
	date = tempBuff;

	_responseHeaders[RTSP_HEADERS]["Date"] = date;
	_responseHeaders[RTSP_HEADERS]["Expires"] = date;
	_responseHeaders[RTSP_HEADERS]["Cache-Control"] = "no-store";
	_responseHeaders[RTSP_HEADERS]["Pragma"] = "no-cache";
	_responseHeaders[RTSP_HEADERS][RTSP_HEADERS_SERVER] = RTSP_HEADERS_SERVER_US;
	_responseHeaders[RTSP_HEADERS].RemoveKey(RTSP_HEADERS_X_POWERED_BY, false);

	//2. send the mesage
	string firstLine = format("%s %u %s",
			STR(_responseHeaders[RTSP_FIRST_LINE][RTSP_VERSION]),
			(uint32_t) _responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE],
			STR(_responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE_REASON]));
	return SendMessage(firstLine, _responseHeaders, _responseContent);
}

OutboundConnectivity * RTSPProtocol::GetOutboundConnectivity(
		BaseInNetStream *pInNetStream, bool forceTcp) {
	if (_pOutboundConnectivity == NULL) {
		BaseOutNetRTPUDPStream *pOutStream = new OutNetRTPUDPH264Stream(this,
				pInNetStream->GetName(), forceTcp);
		if (!pOutStream->SetStreamsManager(GetApplication()->GetStreamsManager())) {
			FATAL("Unable to set the streams manager");
			delete pOutStream;
			pOutStream = NULL;
			return NULL;
		}

		_pOutboundConnectivity = new OutboundConnectivity(forceTcp, this);
		if (!_pOutboundConnectivity->Initialize()) {
			FATAL("Unable to initialize outbound connectivity");
			return NULL;
		}
		pOutStream->SetConnectivity(_pOutboundConnectivity);
		_pOutboundConnectivity->SetOutStream(pOutStream);

		if (!pInNetStream->Link(pOutStream)) {
			FATAL("Unable to link streams");
			return NULL;
		}
	}

	return _pOutboundConnectivity;
}

void RTSPProtocol::CloseOutboundConnectivity() {
	if (_pOutboundConnectivity != NULL) {
		delete _pOutboundConnectivity;
		_pOutboundConnectivity = NULL;
	}
}

InboundConnectivity *RTSPProtocol::GetInboundConnectivity(string sdpStreamName,
		uint32_t bandwidthHint, uint8_t rtcpDetectionInterval) {
	CloseInboundConnectivity();
	string streamName;
	if (GetCustomParameters().HasKey("localStreamName")) {
		streamName = (string) GetCustomParameters()["localStreamName"];
	} else {
		streamName = sdpStreamName;
	}
	_pInboundConnectivity = new InboundConnectivity(this, streamName,
			bandwidthHint, rtcpDetectionInterval);
	return _pInboundConnectivity;
}

InboundConnectivity *RTSPProtocol::GetInboundConnectivity() {
	return _pInboundConnectivity;
}

void RTSPProtocol::CloseInboundConnectivity() {
	if (_pInboundConnectivity != NULL) {
		delete _pInboundConnectivity;
		_pInboundConnectivity = NULL;
	}
}

bool RTSPProtocol::SendRaw(uint8_t *pBuffer, uint32_t length, bool allowDrop) {
	if ((allowDrop)&&(length > 0)) {
		if (GETAVAILABLEBYTESCOUNT(_outputBuffer) > _maxBufferSize) {
			//			WARN("%"PRIu32" bytes dropped on connection %s. Max: %"PRIu32"; Current: %"PRIu32,
			//					length, STR(*this), _maxBufferSize, GETAVAILABLEBYTESCOUNT(_outputBuffer));
			return true;
		}
	}
	_outputBuffer.ReadFromBuffer(pBuffer, length);
	return EnqueueForOutbound();
}

bool RTSPProtocol::SendRaw(MSGHDR *pMessage, uint16_t length, RTPClient *pClient,
		bool isAudio, bool isData, bool allowDrop) {
	if ((allowDrop)&&(length > 0)) {
		if (GETAVAILABLEBYTESCOUNT(_outputBuffer) > _maxBufferSize) {
			//			WARN("%"PRIu32" bytes dropped on connection %s. Max: %"PRIu32"; Current: %"PRIu32,
			//					length, STR(*this), _maxBufferSize, GETAVAILABLEBYTESCOUNT(_outputBuffer));
			return true;
		}
	}

	_outputBuffer.ReadFromByte((uint8_t) '$');
	if (isAudio) {
		if (isData) {
			_outputBuffer.ReadFromByte(pClient->audioDataChannel);
		} else {
			_outputBuffer.ReadFromByte(pClient->audioRtcpChannel);
		}
	} else {
		if (isData) {
			_outputBuffer.ReadFromByte(pClient->videoDataChannel);
		} else {
			_outputBuffer.ReadFromByte(pClient->videoRtcpChannel);
		}
	}
	length = EHTONS(length);
	_outputBuffer.ReadFromBuffer((uint8_t *) & length, 2);
	for (int i = 0; i < (int) pMessage->MSGHDR_MSG_IOVLEN; i++) {
		_outputBuffer.ReadFromBuffer((uint8_t*) pMessage->MSGHDR_MSG_IOV[i].IOVEC_IOV_BASE,
				pMessage->MSGHDR_MSG_IOV[i].IOVEC_IOV_LEN);
	}
	return EnqueueForOutbound();
}

void RTSPProtocol::SetOutStream(BaseOutStream *pOutStream) {
	if (_pOutStream != NULL) {
		delete _pOutStream;
		_pOutStream = NULL;
	}
	_pOutStream = pOutStream;
}

bool RTSPProtocol::SignalProtocolCreated(BaseProtocol *pProtocol,
		Variant &parameters) {
	RTSPProtocol *pRTSPProtocol =
			(RTSPProtocol *) ProtocolManager::GetProtocol(
			(uint32_t) parameters["rtspProtocolId"]);
	if (pRTSPProtocol == NULL) {
		FATAL("RTSP protocol expired");
		if (pProtocol != NULL)
			pProtocol->EnqueueForDelete();
		return false;
	}

	return pRTSPProtocol->SignalPassThroughProtocolCreated((PassThroughProtocol *) pProtocol, parameters);
}

bool RTSPProtocol::SignalPassThroughProtocolCreated(PassThroughProtocol *pProtocol,
		Variant &parameters) {
	if (pProtocol == NULL) {
		FATAL("Connect failed");
		EnqueueForDelete();
		return false;
	}

	_passThroughProtocolId = pProtocol->GetId();

	string payload = parameters["payload"];

	if (!pProtocol->SendTCPData(payload)) {
		FATAL("Unable to send");
		pProtocol->EnqueueForDelete();
		EnqueueForDelete();
		return false;
	}

	return true;
}

bool RTSPProtocol::SendMessage(string &firstLine, Variant &headers, string &content) {
	bool isHttpRequest = (headers[RTSP_FIRST_LINE][RTSP_METHOD] == HTTP_METHOD_GET)
			|| (headers[RTSP_FIRST_LINE][RTSP_METHOD] == HTTP_METHOD_POST);

	//1. Put the first line
	string rawRequest = firstLine + "\r\n";

	//2. Add the content length if required
	if (content.size() > 0) {
		headers[RTSP_HEADERS][RTSP_HEADERS_CONTENT_LENGTH] = format("%"PRIz"u", content.size());
	}

	//3. Add the session id if necessary
	if (!isHttpRequest) {
		if (_sessionId != "") {
			headers[RTSP_HEADERS][RTSP_HEADERS_SESSION] = _sessionId;
		}
	}

	//3. Write the headers

	FOR_MAP(headers[RTSP_HEADERS], string, Variant, i) {
		rawRequest += MAP_KEY(i) + ": " + (string) MAP_VAL(i) + "\r\n";
	}
	rawRequest += "\r\n";

	//4. Write the content
	rawRequest += content;

	if ((isHttpRequest) || (!_isHTTPTunneled)) {
#ifdef RTSP_DUMP_TRAFFIC
		FINEST("OUTPUT\n`%s`", STR(rawRequest));
#endif /* RTSP_DUMP_TRAFFIC */
		_outputBuffer.ReadFromString(rawRequest);

		//5. Enqueue for outbound
		return EnqueueForOutbound();
	} else {
		PassThroughProtocol *pProtocol =
				(PassThroughProtocol *) ProtocolManager::GetProtocol(_passThroughProtocolId);
		if (pProtocol == NULL) {
			string headers = HTTP_METHOD_POST" " + _httpTunnelUri + " "HTTP_VERSION_1_0"\r\n";
			headers += RTSP_HEADERS_X_POWERED_BY": "RTSP_HEADERS_X_POWERED_BY_US"\r\n";
			headers += "x-sessioncookie: " + _xSessionCookie + "\r\n";
			headers += "Content-Type: application/x-rtsp-tunnelled\r\n";
			headers += "Pragma: no-cache\r\n";
			headers += "Cache-Control: no-cache\r\n";
			headers += "Host: " + _httpTunnelHostPort + "\r\n";
			Variant &auth = _authentication["rtsp"];
			if (auth == V_MAP) {
				if (!HTTPAuthHelper::GetAuthorizationHeader(
						(string) auth["authenticateHeader"],
						(string) auth["userName"],
						(string) auth["password"],
						_httpTunnelUri,
						HTTP_METHOD_POST,
						auth["temp"])) {
					FATAL("Unable to create authentication header");
					return false;
				}
				headers += HTTP_HEADERS_AUTORIZATION": " + (string) auth["temp"]["authorizationHeader"]["raw"] + "\r\n";
			}
			headers += "Content-Length: 536870912\r\n\r\n";
#ifdef RTSP_DUMP_TRAFFIC
			FINEST("OUTPUT\n`%s`", STR(headers + "b64(" + rawRequest + ")"));
#endif /* RTSP_DUMP_TRAFFIC */
			rawRequest = headers + b64(rawRequest);

			vector<uint64_t> chain = ProtocolFactoryManager::ResolveProtocolChain(
					CONF_PROTOCOL_TCP_PASSTHROUGH);
			if (chain.size() == 0) {
				FATAL("Unable to resolve protocol chain");
				return false;
			}

			string ip = GetCustomParameters()["customParameters"]["httpProxy"]["ip"];
			uint16_t port = (uint16_t) GetCustomParameters()["customParameters"]["httpProxy"]["port"];

			Variant customParameters;
			customParameters["rtspProtocolId"] = GetId();
			customParameters["payload"] = rawRequest;

			if (!TCPConnector<RTSPProtocol>::Connect(ip, port, chain,
					customParameters)) {
				FATAL("Unable to connect to %s:%"PRIu16, STR(ip), port);
				return false;
			}

			return true;
		} else {
#ifdef RTSP_DUMP_TRAFFIC
			FINEST("OUTPUT\n`%s`", STR("b64(" + rawRequest + ")"));
#endif /* RTSP_DUMP_TRAFFIC */
			rawRequest = b64(rawRequest);
			if (!pProtocol->SendTCPData(rawRequest)) {
				FATAL("Unable to send data");
				pProtocol->EnqueueForDelete();
				EnqueueForDelete();
				return false;
			}

			return true;
		}
	}
}

bool RTSPProtocol::ParseHeaders(IOBuffer& buffer) {
	if (GETAVAILABLEBYTESCOUNT(buffer) < 1) {
		FINEST("Not enough data");
		return true;
	}

	if (GETIBPOINTER(buffer)[0] == '$') {
		return ParseInterleavedHeaders(buffer);
	} else {
		return ParseNormalHeaders(buffer);
	}
}

bool RTSPProtocol::ParseInterleavedHeaders(IOBuffer &buffer) {
	//1. Marl this as a interleaved content
	_rtpData = true;

	//2. Do we have at least 4 bytes ($ sign, channel byte an 2-bytes length)?
	uint32_t bufferLength = GETAVAILABLEBYTESCOUNT(buffer);
	if (bufferLength < 4) {
		return true;
	}

	//3. Get the buffer
	uint8_t *pBuffer = GETIBPOINTER(buffer);

	//4. Get the channel id
	_rtpDataChanel = pBuffer[1];

	//5. Get the packet length
	_rtpDataLength = ENTOHSP(pBuffer + 2);
	if (_rtpDataLength > 8192) {
		FATAL("RTP data length too big");
		return false;
	}

	//6. Do we have enough data?
	if (_rtpDataLength + 4 > bufferLength) {
		return true;
	}

	//7. Advance the state and and ignore the RTP header
	buffer.Ignore(4);
	_state = RTSP_STATE_PAYLOAD;

	//8. Done

	return true;
}

bool RTSPProtocol::ParseNormalHeaders(IOBuffer &buffer) {
	_inboundHeaders.Reset();
	_inboundContent = "";
	//1. We have to have at least 4 bytes (double \r\n)
	if (GETAVAILABLEBYTESCOUNT(buffer) < 4) {
		return true;
	}

	//2. Detect the headers boundaries
	uint32_t headersSize = 0;
	bool markerFound = false;
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	for (uint32_t i = 0; i <= GETAVAILABLEBYTESCOUNT(buffer) - 4; i++) {
		if ((pBuffer[i] == 0x0d)
				&& (pBuffer[i + 1] == 0x0a)
				&& (pBuffer[i + 2] == 0x0d)
				&& (pBuffer[i + 3] == 0x0a)) {
			markerFound = true;
			headersSize = i;
			break;
		}
		if (i >= RTSP_MAX_HEADERS_SIZE) {
			FATAL("Headers section too long");
			return false;
		}
	}

	//3. Are the boundaries correct?
	//Do we have enough data to parse the headers?
	if (headersSize == 0) {
		if (markerFound)
			return false;
		else
			return true;
	}

	//4. Get the raw headers and plit it into lines
	string rawHeaders = string((char *) GETIBPOINTER(buffer), headersSize);
#ifdef RTSP_DUMP_TRAFFIC
	_debugInputData = rawHeaders + "\r\n\r\n";
#endif /* RTSP_DUMP_TRAFFIC */
	vector<string> lines;
	split(rawHeaders, "\r\n", lines);
	if (lines.size() == 0) {
		FATAL("Incorrect RTSP request");
		return false;
	}

	//4. Get the fisrt line and parse it. This is either a status code
	//for a previous request made by us, or the request that we just received
	if (!ParseFirstLine(lines[0])) {
		FATAL("Unable to parse the first line");
		return false;
	}

	//5. Consider the rest of the lines as key: value pairs and store them
	//0. Reset the headers
	_inboundHeaders[RTSP_HEADERS].IsArray(false);
	for (uint32_t i = 1; i < lines.size(); i++) {
		string line = lines[i];
		string splitter = ": ";
		string::size_type splitterPos = line.find(splitter);

		if ((splitterPos == string::npos)
				|| (splitterPos == 0)
				|| (splitterPos == line.size() - splitter.length())) {
			splitter = ":";
			splitterPos = line.find(splitter);
			if ((splitterPos == string::npos)
					|| (splitterPos == 0)
					|| (splitterPos == line.size() - splitter.length())) {
				WARN("Invalid header line: %s", STR(line));
				continue;
			}
		}
		_inboundHeaders[RTSP_HEADERS][line.substr(0, splitterPos)] =
				line.substr(splitterPos + splitter.length(), string::npos);
	}

	//6. default a transfer type to Content-Length: 0 if necessary
	if (!_inboundHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_CONTENT_LENGTH, false)) {
		_inboundHeaders[RTSP_HEADERS][RTSP_HEADERS_CONTENT_LENGTH] = "0";
	}

	//7. read the transfer type and set this request or response flags
	string contentLengthString = _inboundHeaders[RTSP_HEADERS].GetValue(
			RTSP_HEADERS_CONTENT_LENGTH, false);
	replace(contentLengthString, " ", "");
	if (!isNumeric(contentLengthString)) {
		FATAL("Invalid RTSP headers:\n%s", STR(_inboundHeaders.ToString()));
		return false;
	}
	_contentLength = atoi(STR(contentLengthString));

	//7. Advance the state and ignore the headers part from the buffer
	_state = RTSP_STATE_PAYLOAD;
	buffer.Ignore(headersSize + 4);

	_rtpData = false;

	return true;
}

bool RTSPProtocol::ParseFirstLine(string &line) {
	vector<string> parts;
	split(line, " ", parts);
	if (parts.size() < 3) {
		FATAL("Incorrect first line: %s", STR(line));
		return false;
	}

	if (parts[0] == RTSP_VERSION_1_0) {
		if (!isNumeric(parts[1])) {
			FATAL("Invalid RTSP code: %s", STR(parts[1]));
			return false;
		}

		string reason = "";
		for (uint32_t i = 2; i < parts.size(); i++) {
			reason += parts[i];
			if (i != parts.size() - 1)
				reason += " ";
		}

		_inboundHeaders[RTSP_FIRST_LINE][RTSP_VERSION] = parts[0];
		_inboundHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE] = (uint32_t) atoi(STR(parts[1]));
		_inboundHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE_REASON] = reason;
		_inboundHeaders["isRequest"] = false;
		_inboundHeaders["isHttp"] = (bool)false;


		return true;

	} else if (parts[0] == HTTP_VERSION_1_0) {
		if (!isNumeric(parts[1])) {
			FATAL("Invalid HTTP code: %s", STR(parts[1]));
			return false;
		}

		string reason = "";
		for (uint32_t i = 2; i < parts.size(); i++) {
			reason += parts[i];
			if (i != parts.size() - 1)
				reason += " ";
		}

		_inboundHeaders[HTTP_FIRST_LINE][HTTP_VERSION] = parts[0];
		_inboundHeaders[HTTP_FIRST_LINE][HTTP_STATUS_CODE] = (uint32_t) atoi(STR(parts[1]));
		_inboundHeaders[HTTP_FIRST_LINE][HTTP_STATUS_CODE_REASON] = reason;
		_inboundHeaders["isRequest"] = false;
		_inboundHeaders["isHttp"] = (bool)true;
		return true;

	} else if ((parts[0] == RTSP_METHOD_DESCRIBE)
			|| (parts[0] == RTSP_METHOD_OPTIONS)
			|| (parts[0] == RTSP_METHOD_PAUSE)
			|| (parts[0] == RTSP_METHOD_PLAY)
			|| (parts[0] == RTSP_METHOD_SETUP)
			|| (parts[0] == RTSP_METHOD_TEARDOWN)
			|| (parts[0] == RTSP_METHOD_RECORD)
			|| (parts[0] == RTSP_METHOD_ANNOUNCE)
			|| (parts[0] == RTSP_METHOD_GET_PARAMETER)
			|| (parts[0] == RTSP_METHOD_SET_PARAMETER)
			|| (parts[0] == HTTP_METHOD_GET)
			|| (parts[0] == HTTP_METHOD_POST)
			) {
		if ((parts[2] != RTSP_VERSION_1_0)
				&&(parts[2] != HTTP_VERSION_1_0)
				&&(parts[2] != HTTP_VERSION_1_1)
				) {
			FATAL("RTSP/HTTP version not supported: %s", STR(parts[2]));
			return false;
		}

		_inboundHeaders[RTSP_FIRST_LINE][RTSP_METHOD] = parts[0];
		_inboundHeaders[RTSP_FIRST_LINE][RTSP_URL] = parts[1];
		_inboundHeaders[RTSP_FIRST_LINE][RTSP_VERSION] = parts[2];
		_inboundHeaders["isRequest"] = true;
		_inboundHeaders["isHttp"] = (bool)((parts[0] == HTTP_METHOD_GET)
				|| (parts[0] == HTTP_METHOD_POST));

		return true;
	} else {
		FATAL("Incorrect first line: %s", STR(line));
		return false;
	}
}

bool RTSPProtocol::HandleRTSPMessage(IOBuffer &buffer) {
	if (_pProtocolHandler == NULL) {
		FATAL("RTSP protocol decoupled from application");
		return false;
	}
	//1. Get the content
	if (_contentLength > 0) {
		if (_contentLength > 1024 * 1024 * 1024) {
			FATAL("Bogus content length: %u", _contentLength);
			return false;
		}
		uint32_t chunkLength = _contentLength - (uint32_t) _inboundContent.size();
		chunkLength = GETAVAILABLEBYTESCOUNT(buffer) < chunkLength ?
				GETAVAILABLEBYTESCOUNT(buffer) : chunkLength;
		_inboundContent += string((char *) GETIBPOINTER(buffer), chunkLength);
#ifdef RTSP_DUMP_TRAFFIC
		_debugInputData += string((char *) GETIBPOINTER(buffer), chunkLength);
#endif /* RTSP_DUMP_TRAFFIC */

		buffer.Ignore(chunkLength);
		if (!((bool)_inboundHeaders["isHttp"])) {
			if (_inboundContent.size() < _contentLength) {
				FINEST("Not enough data. Wanted: %u; got: %"PRIz"u", _contentLength, _inboundContent.size());
				return true;
			}
		}
	}

	bool result;

	//2. Call the protocol handler
	if ((bool)_inboundHeaders["isRequest"]) {
		if ((bool)_inboundHeaders["isHttp"]) {
			uint32_t before = (uint32_t) _inboundContent.size();
			result = _pProtocolHandler->HandleHTTPRequest(this, _inboundHeaders,
					_inboundContent);
			uint32_t after = (uint32_t) _inboundContent.size();
			if (after > before) {
				FATAL("Data added to content");
				return false;
			}
			if (_contentLength == before) {
				_state = RTSP_STATE_HEADERS;
			} else {
				if (_contentLength < (before - after)) {
					FATAL("Invalid content length detected");
					return false;
				}
				_contentLength -= (before - after);
			}
			if (_contentLength == 0) {
				_state = RTSP_STATE_HEADERS;
			}
		} else {
#ifdef RTSP_DUMP_TRAFFIC
			WARN("INPUT\n`%s`", STR(_debugInputData));
			_debugInputData = "";
#endif /* RTSP_DUMP_TRAFFIC */
			result = _pProtocolHandler->HandleRTSPRequest(this, _inboundHeaders,
					_inboundContent);
			_state = RTSP_STATE_HEADERS;
		}
	} else {
#ifdef RTSP_DUMP_TRAFFIC
		WARN("INPUT\n`%s`", STR(_debugInputData));
		_debugInputData = "";
#endif /* RTSP_DUMP_TRAFFIC */
		if ((bool)_inboundHeaders["isHttp"]) {
			result = _pProtocolHandler->HandleHTTPResponse(this, _inboundHeaders, _inboundContent);
		} else {
			result = _pProtocolHandler->HandleRTSPResponse(this, _inboundHeaders, _inboundContent);
		}
		_state = RTSP_STATE_HEADERS;
	}

	return result;
}

#endif /* HAS_PROTOCOL_RTP */
