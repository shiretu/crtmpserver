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
#include "streaming/basestream.h"
#include "protocols/rtp/basertspappprotocolhandler.h"
#include "protocols/rtp/rtspprotocol.h"
#include "streaming/streamstypes.h"
#include "streaming/baseinnetstream.h"
#include "application/baseclientapplication.h"
#include "protocols/rtp/streaming/baseoutnetrtpudpstream.h"
#include "protocols/rtp/streaming/outnetrtpudph264stream.h"
#include "netio/netio.h"
#include "protocols/rtp/connectivity/outboundconnectivity.h"
#include "application/clientapplicationmanager.h"
#include "protocols/http/httpauthhelper.h"
#include "protocols/rtp/connectivity/inboundconnectivity.h"
#include "streaming/codectypes.h"
#include "protocols/protocolmanager.h"

BaseRTSPAppProtocolHandler::BaseRTSPAppProtocolHandler(Variant &configuration)
: BaseAppProtocolHandler(configuration) {
	_lastUsersFileUpdate = 0;
	_authenticatePlay = false;
}

BaseRTSPAppProtocolHandler::~BaseRTSPAppProtocolHandler() {
}

bool BaseRTSPAppProtocolHandler::ParseAuthenticationNode(Variant &node,
		Variant &result) {
	//1. Users file validation
	string usersFile = node[CONF_APPLICATION_AUTH_USERS_FILE];
	if (!isAbsolutePath(usersFile)) {
		usersFile = (string) _configuration[CONF_APPLICATION_DIRECTORY] + usersFile;
	}
	if (!fileExists(usersFile)) {
		FATAL("Invalid authentication configuration. Missing users file: %s", STR(usersFile));
		return false;
	}
	_usersFile = usersFile;

	if (node.HasKeyChain(V_BOOL, false, 1, "authenticatePlay"))
		_authenticatePlay = (bool)node.GetValue("authenticatePlay", false);

	if (!ParseUsersFile()) {
		FATAL("Unable to parse users file %s", STR(usersFile));
		return false;
	}

	return true;
}

void BaseRTSPAppProtocolHandler::RegisterProtocol(BaseProtocol *pProtocol) {
	//1. Is this a client RTSP protocol?
	if (pProtocol->GetType() != PT_RTSP)
		return;
	Variant &parameters = pProtocol->GetCustomParameters();
	if ((!parameters.HasKeyChain(V_BOOL, true, 1, "isClient"))
			|| (!((bool)parameters["isClient"]))) {
		return;
	}

	//2. Get the pull/push stream config
	if ((!parameters.HasKeyChain(V_MAP, true, 2, "customParameters", "externalStreamConfig"))
			&& (!parameters.HasKeyChain(V_MAP, true, 2, "customParameters", "localStreamConfig"))) {
		WARN("Bogus connection. Terminate it");
		pProtocol->EnqueueForDelete();
		return;
	}
	Variant &streamConfig = (parameters["connectionType"] == "pull") ?
			parameters["customParameters"]["externalStreamConfig"]
			: parameters["customParameters"]["localStreamConfig"];

	//3. remove the forceReconnect flag
	streamConfig.RemoveKey("forceReconnect", false);

	//4. Get the RTSP protocol for more comfortable
	RTSPProtocol *pRTSPProtocol = (RTSPProtocol *) pProtocol;

	//5. validate the networking mode and enable sendRenewStream
	if (parameters.HasKey("forceTcp")) {
		if (parameters["forceTcp"] != V_BOOL) {
			FATAL("Invalid forceTcp flag detected");
			pRTSPProtocol->EnqueueForDelete();
			return;
		}
	} else {
		parameters["forceTcp"] = (bool)false;
	}

	//6. See if we are in HTTP mode
	if ((parameters.HasKeyChain(V_STRING, true, 3, "customParameters", "httpProxy", "ip"))
			&&(parameters.HasKeyChain(_V_NUMERIC, true, 3, "customParameters", "httpProxy", "port"))) {
		parameters["forceTcp"] = (bool)true;
		pRTSPProtocol->IsHTTPTunneled(true);
	}

	if (pRTSPProtocol->IsHTTPTunneled()) {
		//7. Open HTTP tunnel, possibly putting auth on top of it as well
		if (streamConfig.HasKeyChain(V_MAP, true, 1, "rtspAuth")) {
			pRTSPProtocol->SetAuthentication(streamConfig["rtspAuth"]["authenticateHeader"],
					streamConfig["rtspAuth"]["userName"],
					streamConfig["rtspAuth"]["password"],
					true);
		}
		if (!pRTSPProtocol->OpenHTTPTunnel()) {
			FATAL("Unable to open HTTP tunnel");
			pProtocol->EnqueueForDelete();
			return;
		}
	} else {
		//8. Start normal operations (pull or push)
		if (!TriggerPlayOrAnnounce(pRTSPProtocol)) {
			FATAL("Unable to initiate play on uri %s",
					STR(parameters["uri"]));
			pRTSPProtocol->EnqueueForDelete();
			return;
		}
	}
}

void BaseRTSPAppProtocolHandler::UnRegisterProtocol(BaseProtocol *pProtocol) {
	Variant &params = pProtocol->GetCustomParameters();
	if ((params.HasKey("sessionCookie"))
			&&(params.HasKey("removeSessionCookie"))
			&&((bool)params["removeSessionCookie"])
			) {
		_httpSessions.erase(params["sessionCookie"]);
	}
}

bool BaseRTSPAppProtocolHandler::PullExternalStream(URI uri, Variant streamConfig) {
	//1. get the chain
	vector<uint64_t> chain = ProtocolFactoryManager::ResolveProtocolChain(
			CONF_PROTOCOL_INBOUND_RTSP);
	if (chain.size() == 0) {
		FATAL("Unable to resolve protocol chain");
		return false;
	}

	string ip = "";
	uint16_t port = 0;
	Variant httpProxy;

	if ((streamConfig.HasKeyChain(V_STRING, true, 1, "httpProxy"))
			&&(streamConfig["httpProxy"] != "")
			&&(streamConfig["httpProxy"] != "self")) {
		URI testUri;
		if (!URI::FromString("http://" + (string) streamConfig["httpProxy"], true, testUri)) {
			FATAL("Unable to resolve http proxy string: %s", STR(streamConfig["httpProxy"]));
			return false;
		}
		ip = testUri.ip();
		port = testUri.port();
		httpProxy["ip"] = ip;
		httpProxy["port"] = (uint16_t) port;
	} else {
		ip = uri.ip();
		port = uri.port();
	}

	if ((streamConfig.HasKeyChain(V_STRING, true, 1, "httpProxy"))
			&&(streamConfig["httpProxy"] == "self")) {
		httpProxy["ip"] = ip;
		httpProxy["port"] = (uint16_t) port;
	}

	//2. Save the app id inside the custom parameters and mark this connection
	//as client connection
	Variant customParameters = streamConfig;
	customParameters["customParameters"]["externalStreamConfig"] = streamConfig;
	customParameters["customParameters"]["httpProxy"] = httpProxy;
	customParameters["isClient"] = (bool)true;
	customParameters["appId"] = GetApplication()->GetId();
	customParameters["uri"] = uri;
	customParameters["connectionType"] = "pull";

	//3. Connect
	if (!TCPConnector<BaseRTSPAppProtocolHandler>::Connect(ip, port, chain,
			customParameters)) {
		FATAL("Unable to connect to %s:%"PRIu16, STR(ip), port);
		return false;
	}

	//4. Done
	return true;
}

bool BaseRTSPAppProtocolHandler::PushLocalStream(Variant streamConfig) {
	//1. get the stream name
	string streamName = (string) streamConfig["localStreamName"];

	//2. Get the streams manager
	StreamsManager *pStreamsManager = GetApplication()->GetStreamsManager();

	//3. Search for all streams named streamName having the type of IN_NET
	map<uint32_t, BaseStream *> streams = pStreamsManager->FindByTypeByName(
			ST_IN_NET, streamName, true, false);
	if (streams.size() == 0) {
		FATAL("Stream %s not found", STR(streamName));
		return false;
	}

	//4. See if inside the returned collection of streams
	//we have something compatible with RTSP
	BaseInStream *pInStream = NULL;

	FOR_MAP(streams, uint32_t, BaseStream *, i) {
		if (MAP_VAL(i)->IsCompatibleWithType(ST_OUT_NET_RTP)) {
			pInStream = (BaseInStream *) MAP_VAL(i);
			break;
		}
	}
	if (pInStream == NULL) {
		WARN("Stream %s not found or is incompatible with RTSP output",
				STR(streamName));
		return false;
	}

	//5. get the chain
	vector<uint64_t> chain = ProtocolFactoryManager::ResolveProtocolChain(
			CONF_PROTOCOL_INBOUND_RTSP);
	if (chain.size() == 0) {
		FATAL("Unable to resolve protocol chain");
		return false;
	}


	string ip = "";
	uint16_t port = 0;
	Variant httpProxy;

	if ((streamConfig.HasKeyChain(V_STRING, true, 1, "httpProxy"))
			&&(streamConfig["httpProxy"] != "")
			&&(streamConfig["httpProxy"] != "self")) {
		URI testUri;
		if (!URI::FromString("http://" + (string) streamConfig["httpProxy"], true, testUri)) {
			FATAL("Unable to resolve http proxy string: %s", STR(streamConfig["httpProxy"]));
			return false;
		}
		ip = testUri.ip();
		port = testUri.port();
		httpProxy["ip"] = ip;
		httpProxy["port"] = (uint16_t) port;
	} else {
		ip = (string) streamConfig["targetUri"]["ip"];
		port = (uint16_t) streamConfig["targetUri"]["port"];
	}

	if ((streamConfig.HasKeyChain(V_STRING, true, 1, "httpProxy"))
			&&(streamConfig["httpProxy"] == "self")) {
		httpProxy["ip"] = ip;
		httpProxy["port"] = port;
	}


	//6. Save the app id inside the custom parameters and mark this connection
	//as client connection
	Variant customParameters = streamConfig;
	customParameters["customParameters"]["localStreamConfig"] = streamConfig;
	customParameters["customParameters"]["localStreamConfig"]["localUniqueStreamId"] = pInStream->GetUniqueId();
	customParameters["customParameters"]["httpProxy"] = httpProxy;
	customParameters["streamId"] = pInStream->GetUniqueId();
	customParameters["isClient"] = (bool)true;
	customParameters["appId"] = GetApplication()->GetId();
	customParameters["uri"] = streamConfig["targetUri"];
	customParameters["connectionType"] = "push";

	//7. Connect
	if (!TCPConnector<BaseRTSPAppProtocolHandler>::Connect(ip, port, chain,
			customParameters)) {
		FATAL("Unable to connect to %s:%"PRIu16, STR(ip), port);
		return false;
	}

	return true;
}

bool BaseRTSPAppProtocolHandler::SignalProtocolCreated(BaseProtocol *pProtocol,
		Variant &parameters) {
	//1. Sanitize
	if (parameters["appId"] != V_UINT32) {
		FATAL("Invalid custom parameters:\n%s", STR(parameters.ToString()));
		return false;
	}

	//2. Get the application
	BaseClientApplication *pApplication = ClientApplicationManager::FindAppById(
			parameters["appId"]);

	if (pProtocol == NULL) {
		FATAL("Connection failed:\n%s", STR(parameters.ToString()));
		return pApplication->OutboundConnectionFailed(parameters);
	}

	//3. Set it up inside the protocol
	pProtocol->SetApplication(pApplication);

	//4. Done
	return true;
}

bool BaseRTSPAppProtocolHandler::HandleHTTPRequest(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent) {
	//1. Find the session cookie
	if (!requestHeaders.HasKeyChain(V_STRING, false, 2, RTSP_HEADERS, "x-sessioncookie")) {
		FATAL("No session cookie found");
		return false;
	}
	string sessionCookie = (string) (requestHeaders.GetValue(RTSP_HEADERS, false)
			.GetValue("x-sessioncookie", false));
	trim(sessionCookie);
	if (sessionCookie == "") {
		FATAL("No session cookie found");
		return false;
	}

	//2. Store it for later use
	pFrom->GetCustomParameters()["sessionCookie"] = sessionCookie;

	//3. If the method is GET, than switch the channel from HTTP to RTSP
	//by just sending HTTP 200 ok with Content-Type: application/x-rtsp-tunnelled
	if (requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD] == HTTP_METHOD_GET) {
		if (MAP_HAS1(_httpSessions, sessionCookie)) {
			FATAL("session cookie duplicated");
			return false;
		}
		string date;
		char tempBuff[128] = {0};
		struct tm tm;
		time_t t = getutctime();
		gmtime_r(&t, &tm);
		strftime(tempBuff, 127, "%a, %d %b %Y %H:%M:%S UTC", &tm);
		date = tempBuff;

		string rawContent = (string) requestHeaders[RTSP_FIRST_LINE][RTSP_VERSION] + " 200 OK\r\n";
		rawContent += RTSP_HEADERS_SERVER": "RTSP_HEADERS_SERVER_US"\r\n";
		rawContent += "Date: " + date + "\r\n";
		rawContent += "Expires: " + date + "\r\n";
		rawContent += HTTP_HEADERS_CONNECTION": "HTTP_HEADERS_CONNECTION_CLOSE"\r\n";
		rawContent += "Cache-Control: no-store\r\n";
		rawContent += "Pragma: no-cache\r\n";
		rawContent += HTTP_HEADERS_CONTENT_TYPE": application/x-rtsp-tunnelled\r\n\r\n";

		if (!pFrom->SendRaw((uint8_t *) rawContent.data(), (uint32_t) rawContent.size(), false)) {
			FATAL("Unable to send data");
			return false;
		}
		_httpSessions[sessionCookie] = pFrom->GetId();
		pFrom->GetCustomParameters()["removeSessionCookie"] = (bool)true;
		return true;
	}

	pFrom->GetCustomParameters()["removeSessionCookie"] = (bool)false;

	//4. Ok, this is the side channel where the client is posting commands.
	//First, we need to get our hands on the real transport connection
	if (!MAP_HAS1(_httpSessions, sessionCookie)) {
		FATAL("session cookie not found");
		return false;
	}
	BaseProtocol *pProtocol = ProtocolManager::GetProtocol(_httpSessions[sessionCookie]);
	if ((pProtocol == NULL) || (pProtocol->GetType() != PT_RTSP)) {
		FATAL("session cookie pointing to a dead protocol");
		_httpSessions.erase(sessionCookie);
		return false;
	}

	//5. Get the data from the HTTP request but make sure we get a multiple of 4
	uint32_t chunkSize = (uint32_t) ((double) requestContent.size() / 4.0)*(uint32_t) 4;
	string data = unb64(requestContent);

	//6. Fix the requestContent by removing parsed data;
	requestContent = requestContent.substr(chunkSize);

	//7. Feed the target protocol
	return ((RTSPProtocol *) pProtocol)->FeedRTSPRequest(data);
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequest(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	string method = requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD];

	//1. we need a CSeq
	if (!requestHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_CSEQ, false)) {
		FATAL("Request doesn't have %s:\n%s", RTSP_HEADERS_CSEQ,
				STR(requestHeaders.ToString()));
		return false;
	}

	//2. Validate session id
	string wantedSessionId = pFrom->GetSessionId();
	if (wantedSessionId != "") {
		string requestSessionId = "";
		vector<string> parts;
		if (!requestHeaders[RTSP_HEADERS].HasKeyChain(V_STRING, false, 1, RTSP_HEADERS_SESSION)) {
			FATAL("No session id:\n%s", STR(requestHeaders.ToString()));
			return false;
		}

		requestSessionId = (string) requestHeaders[RTSP_HEADERS].GetValue(
				RTSP_HEADERS_SESSION, false);
		split(requestSessionId, ";", parts);
		if (parts.size() >= 1)
			requestSessionId = parts[0];

		if (requestSessionId != wantedSessionId) {
			FATAL("Invalid session ID. Wanted: `%s`; Got: `%s`",
					STR(wantedSessionId),
					STR(requestSessionId));
			return false;
		}
	}

	//4. Prepare a fresh new response. Add the sequence number
	pFrom->ClearResponseMessage();
	pFrom->PushResponseHeader(RTSP_HEADERS_CSEQ,
			requestHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_CSEQ, false));

	//5. Do we have authentication? We will authenticate everything except "OPTIONS"
	if ((_usersFile != "") && NeedAuthentication(pFrom, requestHeaders,
			requestContent)) {
		//6. Re-parse authentication file if necessary
		if (!ParseUsersFile()) {
			FATAL("Unable to parse authentication file");
			return false;
		}

		//7. Get the real name to use it further in authentication process
		string realmName = GetAuthenticationRealm(pFrom, requestHeaders,
				requestContent);

		//8. Do we have that realm?
		if (!_realms.HasKey(realmName)) {
			FATAL("Realm `%s` not found", STR(realmName));
			return false;
		}
		Variant &realm = _realms[realmName];

		//9. Is the user even trying to authenticate?
		if (!requestHeaders[RTSP_HEADERS].HasKey(
				RTSP_HEADERS_AUTHORIZATION, false)) {
			return SendAuthenticationChallenge(pFrom, realm);
		} else {
			//14. The client sent us some response. Validate it now
			//Did we ever sent him an authorization challange?
			if (!pFrom->GetCustomParameters().HasKey("wwwAuthenticate")) {
				FATAL("Client tried to authenticate and the server didn't required that");
				return false;
			}

			//15. Get the server challenge
			string wwwAuthenticate = pFrom->GetCustomParameters()["wwwAuthenticate"];

			//16. Get the client response
			string authorization = (string) requestHeaders[RTSP_HEADERS].GetValue(
					RTSP_HEADERS_AUTHORIZATION, false);

			//17. Try to authenticate
			if (!HTTPAuthHelper::ValidateAuthRequest(wwwAuthenticate,
					authorization,
					method,
					(string) requestHeaders[RTSP_FIRST_LINE][RTSP_URL],
					realm)) {
				WARN("Authorization failed: challenge: %s; response: %s",
						STR(wwwAuthenticate), STR(authorization));
				return SendAuthenticationChallenge(pFrom, realm);
			}

			//18. Success. User authenticated
			//INFO("User authenticated: %s", STR(authorization));
		}
	}

	if (method == RTSP_METHOD_OPTIONS) {
		if (!HandleRTSPRequestOptions(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else if (method == RTSP_METHOD_DESCRIBE) {
		if (!HandleRTSPRequestDescribe(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else if (method == RTSP_METHOD_SETUP) {
		if (!HandleRTSPRequestSetup(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else if ((method == RTSP_METHOD_PLAY) || (method == RTSP_METHOD_RECORD)) {
		if (!HandleRTSPRequestPlayOrRecord(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else if (method == RTSP_METHOD_TEARDOWN) {
		if (!HandleRTSPRequestTearDown(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else if (method == RTSP_METHOD_ANNOUNCE) {
		if (!HandleRTSPRequestAnnounce(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else if (method == RTSP_METHOD_PAUSE) {
		if (!HandleRTSPRequestPause(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else if (method == RTSP_METHOD_SET_PARAMETER) {
		if (!HandleRTSPRequestSetParameter(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else if (method == RTSP_METHOD_GET_PARAMETER) {
		if (!HandleRTSPRequestGetParameter(pFrom, requestHeaders, requestContent)) {
			return false;
		}
	} else {
		FINEST("requestHeaders:\n%s", STR(requestHeaders.ToString()));
		FINEST("requestContent:\n`%s`", STR(requestContent));
		FATAL("Method not implemented yet:\n%s", STR(requestHeaders.ToString()));
		return false;
	}

	return true;
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse(RTSPProtocol *pFrom,
		Variant &responseHeaders, string &responseContent) {
	if (responseHeaders[RTSP_HEADERS].HasKeyChain(V_STRING, false, 1, RTSP_HEADERS_SESSION)) {
		string sessionId = (string) responseHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_SESSION, false);
		if (!pFrom->SetSessionId(sessionId)) {
			FATAL("Unable to set sessionId");
			return false;
		}
	}

	//1. Test the sequence
	if (!responseHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_CSEQ, false)) {
		FATAL("Invalid response:\n%s", STR(responseHeaders.ToString()));
		return false;
	}

	uint32_t seqId = atoi(STR(responseHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_CSEQ, false)));
	Variant requestHeaders;
	string requestContent;
	if (!pFrom->GetRequest(seqId, requestHeaders, requestContent)) {
		FATAL("Invalid response sequence");
		return false;
	}

	//2. Get the request, get the response and call the stack further
	return HandleRTSPResponse(pFrom,
			requestHeaders,
			requestContent,
			responseHeaders,
			responseContent);
}

bool BaseRTSPAppProtocolHandler::HandleHTTPResponse(RTSPProtocol *pFrom, Variant &responseHeaders,
		string &responseContent) {
	Variant requestHeaders;
	string requestContent;
	if (!pFrom->GetRequest(0xffffffff, requestHeaders, requestContent)) {
		FATAL("Invalid response sequence");
		return false;
	}

	//2. Get the request, get the response and call the stack further
	return HandleHTTPResponse(pFrom,
			requestHeaders,
			requestContent,
			responseHeaders,
			responseContent);
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestOptions(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	pFrom->PushResponseHeader(RTSP_HEADERS_PUBLIC, "DESCRIBE, OPTIONS, PAUSE, PLAY, SETUP, TEARDOWN, ANNOUNCE, RECORD");
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestDescribe(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	//1. Analyze the requested URI and get all the flags and the stream name
	if (!AnalyzeUri(pFrom, (string) requestHeaders[RTSP_FIRST_LINE][RTSP_URL])) {
		FATAL("URI analyzer failed");
		return false;
	}
	string streamName = GetStreamName(pFrom);

	//2. Get the inbound stream capabilities
	BaseInStream *pInStream = GetInboundStream(streamName, pFrom);


	//3. Prepare the body of the response
	string outboundContent = ComputeSDP(pFrom, streamName, "", false);
	if (outboundContent == "") {
		FATAL("Unable to compute SDP");
		return false;
	}

	//4. Save the stream id for later usage
	pFrom->GetCustomParameters()["streamId"] = pInStream->GetUniqueId();

	//5. Mark this connection as outbound connection
	pFrom->GetCustomParameters()["isInbound"] = false;

	//6. prepare the response
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	pFrom->PushResponseHeader(RTSP_HEADERS_CONTENT_TYPE, RTSP_HEADERS_ACCEPT_APPLICATIONSDP);
	string contentBase = (string) requestHeaders[RTSP_FIRST_LINE][RTSP_URL];
	if (contentBase != "") {
		if (contentBase[contentBase.length() - 1] != '/')
			contentBase += "/";
		pFrom->PushResponseHeader(RTSP_HEADERS_CONTENT_BASE, contentBase);
	}
	pFrom->PushResponseContent(outboundContent, false);

	//7. Done
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestSetup(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	if (pFrom->GetCustomParameters()["isInbound"] != V_BOOL) {
		FATAL("Invalid state");
		return false;
	}

	if ((bool)pFrom->GetCustomParameters()["isInbound"])
		return HandleRTSPRequestSetupInbound(pFrom, requestHeaders, requestContent);
	else
		return HandleRTSPRequestSetupOutbound(pFrom, requestHeaders, requestContent);
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestSetupOutbound(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	//1. Minimal sanitize
	if (!requestHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_TRANSPORT, false)) {
		FATAL("RTSP %s request doesn't have %s header line",
				RTSP_METHOD_SETUP,
				RTSP_HEADERS_TRANSPORT);
		return false;
	}

	Variant &customParams = pFrom->GetCustomParameters();
	if (!customParams.HasKey("nextChannel"))
		customParams["nextChannel"] = (uint16_t) 0;

	//2. get the transport header line
	string raw = requestHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_TRANSPORT, false);
	Variant transports;
	Variant transport;
	if (!SDP::ParseTransportLine(raw, transports)) {
		FATAL("Unable to parse transport line %s", STR(raw));
		return false;
	}
	bool forceTcp = false;

	FOR_MAP(transports["alternatives"], string, Variant, i) {
		Variant &temp = MAP_VAL(i);
		if (temp.HasKey("client_port")
				&& (temp.HasKey("rtp/avp/udp") || temp.HasKey("rtp/avp"))) {
			forceTcp = false;
			transport = temp;
			break;
		} else if (temp.HasKey("interleaved") && temp.HasKey("rtp/avp/tcp")) {
			forceTcp = true;
			transport = temp;
			break;
		} else if (temp.HasKey("rtp/avp/tcp")) {
			forceTcp = true;
			transport = temp;
			uint16_t channel = (uint16_t) customParams["nextChannel"];
			transport["interleaved"]["all"] = format("%"PRIu16"-%"PRIu16, channel, channel + 1);
			transport["interleaved"]["data"] = (uint16_t) channel;
			transport["interleaved"]["rtcp"] = (uint16_t) (channel + 1);
			customParams["nextChannel"] = (uint16_t) (channel + 2);
			break;
		}
	}
	if (transport == V_NULL) {
		FATAL("Unable to find a valid transport alternative:\n%s", STR(transports.ToString()));
		return false;
	}

	customParams["forceTcp"] = (bool)forceTcp;

	//3. Get the outbound connectivity
	OutboundConnectivity *pOutboundConnectivity = GetOutboundConnectivity(pFrom,
			forceTcp);
	if (pOutboundConnectivity == NULL) {
		FATAL("Unable to get the outbound connectivity");
		return false;
	}

	//5. Find out if this is audio or video
	bool isAudioTrack = false;
	string rawUrl = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];
	string audioTrackId;
	if (customParams.HasKey("audioTrackId"))
		audioTrackId = "trackID=" + (string) customParams["audioTrackId"];
	string videoTrackId;
	if (customParams.HasKey("videoTrackId"))
		videoTrackId = "trackID=" + (string) customParams["videoTrackId"];
	if ((audioTrackId != "") && (rawUrl.find(audioTrackId) != string::npos)) {
		isAudioTrack = true;
	} else if ((videoTrackId != "") && (rawUrl.find(videoTrackId) != string::npos)) {
		isAudioTrack = false;
	} else {
		FATAL("Invalid track. Wanted: %s or %s; Got: %s",
				STR(audioTrackId),
				STR(videoTrackId),
				STR(requestHeaders[RTSP_FIRST_LINE][RTSP_URL]));
		return false;
	}
	customParams["isAudioTrack"] = (bool)isAudioTrack;
	if (isAudioTrack) {
		if (forceTcp) {
			customParams["audioDataChannelNumber"] = transport["interleaved"]["data"];
			customParams["audioRtcpChannelNumber"] = transport["interleaved"]["rtcp"];
			customParams["audioTrackUri"] = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];
			pOutboundConnectivity->HasAudio(true);
		} else {
			customParams["audioDataPortNumber"] = transport["client_port"]["data"];
			customParams["audioRtcpPortNumber"] = transport["client_port"]["rtcp"];
			customParams["audioTrackUri"] = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];
			pOutboundConnectivity->HasAudio(true);
		}
	} else {
		if (forceTcp) {
			customParams["videoDataChannelNumber"] = transport["interleaved"]["data"];
			customParams["videoRtcpChannelNumber"] = transport["interleaved"]["rtcp"];
			customParams["videoTrackUri"] = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];
			pOutboundConnectivity->HasVideo(true);
		} else {
			customParams["videoDataPortNumber"] = transport["client_port"]["data"];
			customParams["videoRtcpPortNumber"] = transport["client_port"]["rtcp"];
			customParams["videoTrackUri"] = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];
			pOutboundConnectivity->HasVideo(true);
		}
	}

	//10. Create a session
	pFrom->GenerateSessionId();

	//10 Compose the response
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	if (forceTcp) {
		pFrom->PushResponseHeader(RTSP_HEADERS_TRANSPORT,
				format("RTP/AVP/TCP;unicast;interleaved=%s",
				STR(transport["interleaved"]["all"])));
	} else {
		pFrom->PushResponseHeader(RTSP_HEADERS_TRANSPORT,
				format("RTP/AVP/UDP;unicast;source=%s;client_port=%s;server_port=%s;ssrc=%08x",
				STR(((TCPCarrier *) pFrom->GetIOHandler())->GetNearEndpointAddressIp()),
				STR(transport["client_port"]["all"]),
				isAudioTrack ? STR(pOutboundConnectivity->GetAudioPorts())
				: STR(pOutboundConnectivity->GetVideoPorts()),
				isAudioTrack ? pOutboundConnectivity->GetAudioSSRC()
				: pOutboundConnectivity->GetVideoSSRC()));
	}

	if (isAudioTrack) {
		customParams["rtpInfo"]["audio"]["url"] = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];
	} else {
		customParams["rtpInfo"]["video"]["url"] = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];
	}

	//10. Done
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestSetupInbound(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	//1. get the transport line and split it into parts
	if (!requestHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_TRANSPORT, false)) {
		FATAL("No transport line");
		return false;
	}
	string transportLine = requestHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_TRANSPORT, false);
	Variant transports;
	Variant transport;
	if (!SDP::ParseTransportLine(transportLine, transports)) {
		FATAL("Unable to parse transport line");
		return false;
	}

	FOR_MAP(transports["alternatives"], string, Variant, i) {
		Variant &temp = MAP_VAL(i);
		//2. Check and see if it has RTP/AVP/TCP,RTP/AVP/UDP or RTP/AVP
		if ((!temp.HasKey("rtp/avp/tcp"))
				&& (!temp.HasKey("rtp/avp/udp"))
				&& (!temp.HasKey("rtp/avp")))
			continue;

		//3. Check to see if it has either client_port OR interleaved
		if ((!temp.HasKey("client_port"))
				&& (!temp.HasKey("interleaved")))
			continue;

		if ((temp.HasKey("client_port"))
				&& (temp.HasKey("interleaved")))
			continue;
		transport = temp;
		break;
	}

	if (transport == V_NULL) {
		FATAL("Unable to find a valid transport alternative:\n%s", STR(transports.ToString()));
		return false;
	}

	//4. Get the InboundConnectivity
	InboundConnectivity *pConnectivity = pFrom->GetInboundConnectivity();

	//4. Find the track inside the pendingTracks collection and setup the ports or channels
	if (pFrom->GetCustomParameters()["pendingTracks"] != V_MAP) {
		FATAL("Invalid state. No pending tracks");
		return false;
	}
	string controlUri = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];

	bool trackFound = false;

	FOR_MAP(pFrom->GetCustomParameters()["pendingTracks"], string, Variant, i) {
		Variant &track = MAP_VAL(i);
		if (track["controlUri"] == controlUri) {
			if (transport.HasKey("client_port")) {
				track["portsOrChannels"] = transport["client_port"];
				track["isTcp"] = (bool)false;
			} else {
				track["portsOrChannels"] = transport["interleaved"];
				track["isTcp"] = (bool)true;
			}
			if (!pConnectivity->AddTrack(track, (bool)track["isAudio"])) {
				FATAL("Unable to add audio track");
				return false;
			}
			transportLine = pConnectivity->GetTransportHeaderLine((bool)track["isAudio"], false);
			trackFound = true;
			break;
		}
	}
	if (!trackFound) {
		FATAL("track %s not found", STR(controlUri));
		return false;
	}

	//5. Create a session
	pFrom->GenerateSessionId();

	//6. prepare the response
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	pFrom->PushResponseHeader(RTSP_HEADERS_TRANSPORT, transportLine);

	//7. Send it
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestTearDown(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	pFrom->EnqueueForDelete();
	return true;
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestAnnounce(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	//1. Make sure we ONLY handle application/sdp
	if (!requestHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_CONTENT_TYPE, false)) {
		FATAL("Invalid ANNOUNCE request:\n%s", STR(requestHeaders.ToString()));
		return false;
	}
	if (requestHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_CONTENT_TYPE, false)
			!= RTSP_HEADERS_ACCEPT_APPLICATIONSDP) {
		FATAL("Invalid ANNOUNCE request:\n%s", STR(requestHeaders.ToString()));
		return false;
	}

	//2. Get the SDP
	SDP &sdp = pFrom->GetInboundSDP();

	//3. Parse the SDP
	if (!SDP::ParseSDP(sdp, requestContent)) {
		FATAL("Unable to parse the SDP");
		return false;
	}

	//4. Get the first video track
	Variant videoTrack = sdp.GetVideoTrack(0,
			requestHeaders[RTSP_FIRST_LINE][RTSP_URL]);
	Variant audioTrack = sdp.GetAudioTrack(0,
			requestHeaders[RTSP_FIRST_LINE][RTSP_URL]);

	//5. Store the tracks inside the session for later use
	if (audioTrack != V_NULL) {
		pFrom->GetCustomParameters()["pendingTracks"][(uint32_t) SDP_TRACK_GLOBAL_INDEX(audioTrack)] = audioTrack;
	}
	if (videoTrack != V_NULL) {
		pFrom->GetCustomParameters()["pendingTracks"][(uint32_t) SDP_TRACK_GLOBAL_INDEX(videoTrack)] = videoTrack;
	}

	//6. Mark this connection as inbound connection
	pFrom->GetCustomParameters()["isInbound"] = true;

	//7. Save the streamName
	string streamName = sdp.GetStreamName();
	if (streamName == "") {
		streamName = format("rtsp_stream_%u", pFrom->GetId());
	}
	pFrom->GetCustomParameters()["sdpStreamName"] = streamName;

	//8. Save the bandwidth hint
	pFrom->GetCustomParameters()["sdpBandwidthHint"] = (uint32_t) sdp.GetTotalBandwidth();

	//9. Get the inbound connectivity
	InboundConnectivity *pInboundConnectivity = pFrom->GetInboundConnectivity(
			streamName,
			sdp.GetTotalBandwidth(),
			(uint8_t) GetApplication()->GetConfiguration()[CONF_APPLICATION_RTCPDETECTIONINTERVAL]);
	if (pInboundConnectivity == NULL) {
		FATAL("Unable to create inbound connectivity");
		return false;
	}

	//8. Send back the response
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestPause(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	string range = "";

	pFrom->GetCustomParameters()["pausePoint"] = (double) 0;
	range = "npt=now-";

	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	if (range != "")
		pFrom->PushResponseHeader(RTSP_HEADERS_RANGE, range);
	EnableDisableOutput(pFrom, false);
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestPlayOrRecord(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	if (pFrom->GetCustomParameters()["isInbound"] != V_BOOL) {
		FATAL("Invalid state");
		return false;
	}
	if ((bool)pFrom->GetCustomParameters()["isInbound"]) {
		return HandleRTSPRequestRecord(pFrom, requestHeaders, requestContent);
	} else {
		return HandleRTSPRequestPlay(pFrom, requestHeaders, requestContent);
	}
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestPlay(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	if ((bool)pFrom->GetCustomParameters()["isInbound"] != false) {
		FATAL("Invalid state");
		return false;
	}

	/*
	 * we have 3 bool values dictating the state we are on
	 * 1. has range header (hasRangeheader)
	 * 2. was paused before (wasPausedBefore)
	 * 3. this is the first play command (fisrtPlayCommand)
	 *
	 * no.	hasRangeheader	wasPausedBefore	fisrtPlayCommand	Outcome
	 *	0		0				0					0			keep-alive
	 *	1		0				0					1			play
	 *	2		0				1					0			play
	 *	3		0				1					1			N/A, play
	 *	4		1				0					0			play *
	 *	5		1				0					1			play *
	 *	6		1				1					0			play *
	 *	7		1				1					1			play *
	 */

	Variant &params = pFrom->GetCustomParameters();
	bool hasRangeheader = requestHeaders[RTSP_HEADERS].HasKeyChain(V_STRING,
			false, 1, RTSP_HEADERS_RANGE);
	bool wasPausedBefore = params.HasKeyChain(V_DOUBLE, true, 1, "pausePoint");
	bool fisrtPlayCommand = params.HasKeyChain(V_BOOL, true, 1, "fisrtPlayCommand") ?
			((bool)params["fisrtPlayCommand"]) : true;

	//	FINEST("hasRangeheader: %d; wasPausedBefore: %d; fisrtPlayCommand: %d",
	//			hasRangeheader, wasPausedBefore, fisrtPlayCommand);

	if (!hasRangeheader) {
		//0,1,2,3
		if (wasPausedBefore) {
			//2,3
			requestHeaders[RTSP_HEADERS][RTSP_HEADERS_RANGE] = format("npt=%.3f-",
					(double) params["pausePoint"]);
		} else {
			//0,1
			if (fisrtPlayCommand) {
				//1
				requestHeaders[RTSP_HEADERS][RTSP_HEADERS_RANGE] = "npt=now-";
			} else {
				//0
				pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
				return pFrom->SendResponseMessage();
			}
		}
	}
	params.RemoveKey("pausePoint");
	params["fisrtPlayCommand"] = (bool)false;

	if (params["isInbound"] != V_BOOL) {
		FATAL("Invalid state");
		return false;
	}

	if ((bool)params["isInbound"]) {
		return HandleRTSPRequestRecord(pFrom, requestHeaders, requestContent);
	}

	//1. Get the outbound connectivity
	bool forceTcp = (bool)params.GetValue("forceTcp", false);
	OutboundConnectivity *pOutboundConnectivity = GetOutboundConnectivity(pFrom, forceTcp);
	if (pOutboundConnectivity == NULL) {
		FATAL("Unable to get the outbound connectivity");
		return false;
	}

	if (forceTcp) {
		//3. Get the audio/video client ports
		uint8_t videoDataChannelNumber = 0xff;
		uint8_t videoRtcpChannelNumber = 0xff;
		uint8_t audioDataChannelNumber = 0xff;
		uint8_t audioRtcpChannelNumber = 0xff;
		if (params.HasKey("audioDataChannelNumber")) {
			audioDataChannelNumber = (uint8_t) params["audioDataChannelNumber"];
			audioRtcpChannelNumber = (uint8_t) params["audioRtcpChannelNumber"];
		}
		if (params.HasKey("videoDataChannelNumber")) {
			videoDataChannelNumber = (uint8_t) params["videoDataChannelNumber"];
			videoRtcpChannelNumber = (uint8_t) params["videoRtcpChannelNumber"];
		}

		//4.register the video
		if (videoDataChannelNumber != 0xff) {
			if (!pOutboundConnectivity->RegisterTCPVideoClient(pFrom->GetId(),
					videoDataChannelNumber, videoRtcpChannelNumber)) {
				FATAL("Unable to register video stream");
				return false;
			}
		}

		//5. Register the audio
		if (audioDataChannelNumber != 0xff) {
			if (!pOutboundConnectivity->RegisterTCPAudioClient(pFrom->GetId(),
					audioDataChannelNumber, audioRtcpChannelNumber)) {
				FATAL("Unable to register audio stream");
				return false;
			}
		}
	} else {
		//3. Get the audio/video client ports
		uint16_t videoDataPortNumber = 0;
		uint16_t videoRtcpPortNumber = 0;
		uint16_t audioDataPortNumber = 0;
		uint16_t audioRtcpPortNumber = 0;
		if (params.HasKey("audioDataPortNumber")) {
			audioDataPortNumber = (uint16_t) params["audioDataPortNumber"];
			audioRtcpPortNumber = (uint16_t) params["audioRtcpPortNumber"];
		}
		if (params.HasKey("videoDataPortNumber")) {
			videoDataPortNumber = (uint16_t) params["videoDataPortNumber"];
			videoRtcpPortNumber = (uint16_t) params["videoRtcpPortNumber"];
		}

		//4.register the video
		if (videoDataPortNumber != 0) {
			sockaddr_in videoDataAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddress();
			videoDataAddress.sin_port = EHTONS(videoDataPortNumber);
			sockaddr_in videoRtcpAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddress();
			videoRtcpAddress.sin_port = EHTONS(videoRtcpPortNumber);
			if (!pOutboundConnectivity->RegisterUDPVideoClient(pFrom->GetId(),
					videoDataAddress, videoRtcpAddress)) {
				FATAL("Unable to register video stream");
				return false;
			}
		}

		//5. Register the audio
		if (audioDataPortNumber != 0) {
			sockaddr_in audioDataAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddress();
			audioDataAddress.sin_port = EHTONS(audioDataPortNumber);
			sockaddr_in audioRtcpAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddress();
			audioRtcpAddress.sin_port = EHTONS(audioRtcpPortNumber);
			if (!pOutboundConnectivity->RegisterUDPAudioClient(pFrom->GetId(),
					audioDataAddress, audioRtcpAddress)) {
				FATAL("Unable to register audio stream");
				return false;
			}
		}
	}

	//6. prepare the response
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	pFrom->PushResponseHeader(RTSP_HEADERS_RANGE, "npt=now-");

	EnableDisableOutput(pFrom, true);

	//7. Done
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestRecord(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	//1. Make sure we have everything and we are in the proper state
	if ((bool)pFrom->GetCustomParameters()["isInbound"] != true) {
		FATAL("Invalid state");
		return false;
	}

	if (pFrom->GetCustomParameters()["pendingTracks"] != V_MAP) {
		FATAL("Invalid state");
		return false;
	}

	//3. Get the inbound connectivity
	InboundConnectivity *pConnectivity = pFrom->GetInboundConnectivity();
	if (pConnectivity == NULL) {
		FATAL("Unable to get inbound connectivity");
		return false;
	}
	if (!pConnectivity->Initialize()) {
		FATAL("Unable to initialize inbound connectivity");
		return false;
	}

	//4. Send back the response
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestSetParameter(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPRequestGetParameter(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 200, "OK");
	return pFrom->SendResponseMessage();
}

bool BaseRTSPAppProtocolHandler::HandleHTTPResponse(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	switch ((uint32_t) responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE]) {
		case 200:
		{
			return HandleHTTPResponse200(pFrom, requestHeaders, requestContent,
					responseHeaders, responseContent);
		}
		case 401:
		{
			return HandleHTTPResponse401(pFrom, requestHeaders, requestContent,
					responseHeaders, responseContent);
		}
		case 404:
		{
			FATAL("Resource not found: %s", STR(requestHeaders[RTSP_FIRST_LINE][RTSP_URL]));
			return false;
		}
		default:
		{
			FATAL("Response not yet implemented. request:\n%s\nresponse:\n%s",
					STR(requestHeaders.ToString()),
					STR(responseHeaders.ToString()));

			return false;
		}
	}
}

bool BaseRTSPAppProtocolHandler::HandleHTTPResponse200(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	string method = requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD];

	//2. Call the appropriate function
	if (method == HTTP_METHOD_GET) {
		return HandleHTTPResponse200Get(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else {
		FATAL("Response for method %s not implemented yet", STR(method));
		return false;
	}
}

bool BaseRTSPAppProtocolHandler::HandleHTTPResponse401(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	string method = requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD];

	if (method == HTTP_METHOD_GET) {
		return HandleHTTPResponse401Get(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else {
		FATAL("Response for method %s not implemented yet", STR(method));
		return false;
	}
}

bool BaseRTSPAppProtocolHandler::HandleHTTPResponse200Get(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	if (!TriggerPlayOrAnnounce(pFrom)) {
		FATAL("Unable to initiate play/announce on uri %s",
				STR(requestHeaders[RTSP_FIRST_LINE][RTSP_URL]));
		return false;
	}
	return true;
}

bool BaseRTSPAppProtocolHandler::HandleHTTPResponse401Get(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	if ((responseHeaders.HasKeyChain(V_STRING, false, 2, RTSP_HEADERS, HTTP_HEADERS_WWWAUTHENTICATE))
			&& (responseHeaders[RTSP_HEADERS][HTTP_HEADERS_WWWAUTHENTICATE] != "")) {
		Variant &customParams = pFrom->GetCustomParameters();
		Variant &params = (customParams["connectionType"] == "pull") ?
				customParams["customParameters"]["externalStreamConfig"]
				: customParams["customParameters"]["localStreamConfig"];
		params["forceReconnect"] = (bool)true;
		params["rtspAuth"]["authenticateHeader"] = responseHeaders[RTSP_HEADERS][HTTP_HEADERS_WWWAUTHENTICATE];
		params["rtspAuth"]["userName"] = params["uri"]["userName"];
		params["rtspAuth"]["password"] = params["uri"]["password"];
	}
	pFrom->EnqueueForDelete();
	return true;
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	switch ((uint32_t) responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE]) {
		case 200:
		{
			return HandleRTSPResponse200(pFrom, requestHeaders, requestContent,
					responseHeaders, responseContent);
		}
		case 401:
		{
			return HandleRTSPResponse401(pFrom, requestHeaders, requestContent,
					responseHeaders, responseContent);
		}
		case 404:
		{
			return HandleRTSPResponse404(pFrom, requestHeaders, requestContent,
					responseHeaders, responseContent);
		}
		default:
		{
			FATAL("Response not yet implemented. request:\n%s\nresponse:\n%s",
					STR(requestHeaders.ToString()),
					STR(responseHeaders.ToString()));

			return false;
		}
	}
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse200(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	//1. Get the method
	string method = requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD];

	//2. Call the appropriate function
	if (method == RTSP_METHOD_OPTIONS) {
		return HandleRTSPResponse200Options(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else if (method == RTSP_METHOD_DESCRIBE) {
		return HandleRTSPResponse200Describe(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else if (method == RTSP_METHOD_SETUP) {
		return HandleRTSPResponse200Setup(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else if (method == RTSP_METHOD_PLAY) {
		return HandleRTSPResponse200Play(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else if (method == RTSP_METHOD_ANNOUNCE) {
		return HandleRTSPResponse200Announce(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else if (method == RTSP_METHOD_RECORD) {
		return HandleRTSPResponse200Record(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else if (method == RTSP_METHOD_GET_PARAMETER) {
		// This is just a fake method for now and primarily used for keepalive
		return true;
	} else if (method == RTSP_METHOD_SET_PARAMETER) {
		// This is just a way of trigering IDR
		return true;
	} else {
		FATAL("Response for method %s not implemented yet", STR(method));
		return false;
	}
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse401(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	if ((!pFrom->GetCustomParameters().HasKeyChain(V_MAP, false, 1, "uri"))
			|| (!pFrom->GetCustomParameters().HasKeyChain(V_STRING, false, 2, "uri", "userName"))
			|| (!pFrom->GetCustomParameters().HasKeyChain(V_STRING, false, 2, "uri", "password"))
			|| (pFrom->GetCustomParameters()["uri"]["userName"] == "")) {
		FATAL("No username/password provided");
		return false;
	}
	if ((!responseHeaders.HasKeyChain(V_STRING, false, 2, RTSP_HEADERS, HTTP_HEADERS_WWWAUTHENTICATE))
			|| (responseHeaders[RTSP_HEADERS][HTTP_HEADERS_WWWAUTHENTICATE] == "")) {
		FATAL("Invalid 401 response: %s", STR(responseHeaders.ToString()));
		return false;
	}
	string username = pFrom->GetCustomParameters()["uri"]["userName"];
	string password = pFrom->GetCustomParameters()["uri"]["password"];
	if (!pFrom->SetAuthentication(
			(string) responseHeaders[RTSP_HEADERS][HTTP_HEADERS_WWWAUTHENTICATE],
			username,
			password,
			true)) {
		FATAL("Unable to authenticate: request headers:\n%s\nresponseHeaders:\n%s",
				STR(requestHeaders.ToString()),
				STR(responseHeaders.ToString()));
		return false;
	}
	return pFrom->SendRequestMessage();
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse404(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	//1. Get the method
	string method = requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD];

	//2. Call the appropriate function
	if (method == RTSP_METHOD_PLAY) {
		return HandleRTSPResponse404Play(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else if (method == RTSP_METHOD_DESCRIBE) {
		return HandleRTSPResponse404Describe(pFrom, requestHeaders, requestContent,
				responseHeaders, responseContent);
	} else {
		FATAL("Response for method %s not implemented yet\n%s", STR(method),
				STR(responseHeaders.ToString()));
		return false;
	}
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse200Options(
		RTSPProtocol *pFrom, Variant &requestHeaders, string &requestContent,
		Variant &responseHeaders, string &responseContent) {
	if (pFrom->HasConnectivity()) {
		//FINEST("This is a keep alive timer....");
		return true;
	}

	if (!pFrom->GetCustomParameters().HasKeyChain(V_STRING, true, 1, "connectionType")) {
		FATAL("Bogus connection");
		pFrom->EnqueueForDelete();
		return false;
	}

	//1. Sanitize
	if (!responseHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_PUBLIC, false)) {
		FATAL("Invalid response:\n%s", STR(responseHeaders.ToString()));
		return false;
	}

	//2. get the raw options
	string raw = responseHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_PUBLIC,
			false);

	//3. split and normalize the options
	map<string, string> parts = mapping(raw, ",", ":", true);
	string url = requestHeaders[RTSP_FIRST_LINE][RTSP_URL];


	if (MAP_HAS1(parts, RTSP_METHOD_GET_PARAMETER)) {
		pFrom->SetKeepAliveMethod(RTSP_METHOD_GET_PARAMETER);
	} else {
		pFrom->SetKeepAliveMethod(RTSP_METHOD_OPTIONS);
	}

	if (pFrom->GetCustomParameters()["connectionType"] == "pull") {
		//4. Test the presence of the wanted methods
		if ((!MAP_HAS1(parts, RTSP_METHOD_DESCRIBE))
				|| (!MAP_HAS1(parts, RTSP_METHOD_SETUP))
				|| (!MAP_HAS1(parts, RTSP_METHOD_PLAY))
				) {
			FATAL("Some of the supported methods are missing: %s", STR(raw));
			return false;
		}

		//5. Prepare the DESCRIBE method
		pFrom->PushRequestFirstLine(RTSP_METHOD_DESCRIBE, url, RTSP_VERSION_1_0);
		pFrom->PushRequestHeader(RTSP_HEADERS_ACCEPT, RTSP_HEADERS_ACCEPT_APPLICATIONSDP);
		return pFrom->SendRequestMessage();

	} else if (pFrom->GetCustomParameters()["connectionType"] == "push") {
		//4. Test the presence of the wanted methods
		if ((!MAP_HAS1(parts, RTSP_METHOD_ANNOUNCE))
				|| (!MAP_HAS1(parts, RTSP_METHOD_SETUP))
				|| (!MAP_HAS1(parts, RTSP_METHOD_RECORD))
				) {
			FATAL("Some of the supported methods are missing: %s", STR(raw));
			return false;
		}
		Variant parameters = pFrom->GetCustomParameters();
		pFrom->PushRequestFirstLine(RTSP_METHOD_ANNOUNCE, url, RTSP_VERSION_1_0);
		string sdp = ComputeSDP(pFrom,
				parameters["customParameters"]["localStreamConfig"]["localStreamName"],
				parameters["customParameters"]["localStreamConfig"]["targetStreamName"],
				true);
		if (sdp == "") {
			FATAL("Unable to compute sdp");
			return false;
		}
		pFrom->PushRequestHeader(RTSP_HEADERS_CONTENT_TYPE, RTSP_HEADERS_ACCEPT_APPLICATIONSDP);
		pFrom->PushRequestContent(sdp, false);
		return pFrom->SendRequestMessage();
	} else {
		FATAL("Bogus connection");
		pFrom->EnqueueForDelete();
		return false;
	}
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse200Describe(
		RTSPProtocol *pFrom, Variant &requestHeaders, string &requestContent,
		Variant &responseHeaders, string &responseContent) {
	//1. Make sure we ONLY handle application/sdp
	if (!responseHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_CONTENT_TYPE, false)) {
		FATAL("Invalid DESCRIBE response:\n%s", STR(requestHeaders.ToString()));
		return false;
	}
	if (responseHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_CONTENT_TYPE, false)
			!= RTSP_HEADERS_ACCEPT_APPLICATIONSDP) {
		FATAL("Invalid DESCRIBE response:\n%s", STR(requestHeaders.ToString()));
		return false;
	}

	string baseUri = "";
	if (responseHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_CONTENT_BASE, false)) {
		baseUri = (string) responseHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_CONTENT_BASE, false);
		trim(baseUri);
	}
	if (baseUri == "") {
		WARN("DESCRIBE response without content base. default it to the base of the URI");
		URI tempURI;
		if (!URI::FromString(requestHeaders[RTSP_FIRST_LINE][RTSP_URL], false, tempURI)) {
			FATAL("Unable to parse URI");
			return false;
		}
		baseUri = tempURI.baseURI();
		if (baseUri == "") {
			FATAL("Unable to get the base URI");
			return false;
		}
	}

	//2. Get the SDP
	SDP &sdp = pFrom->GetInboundSDP();

	//3. Parse the SDP
	if (!SDP::ParseSDP(sdp, responseContent)) {
		FATAL("Unable to parse the SDP");
		return false;
	}

	//4. Get the first video track
	Variant videoTrack = sdp.GetVideoTrack(0, baseUri);
	Variant audioTrack = sdp.GetAudioTrack(0, baseUri);
	if ((videoTrack == V_NULL) && (audioTrack == V_NULL)) {
		FATAL("No compatible tracks found");
		return false;
	}

	bool forceTcp = false;
	if (pFrom->GetCustomParameters().HasKeyChain(V_BOOL, true, 1, "forceTcp"))
		forceTcp = (bool)pFrom->GetCustomParameters()["forceTcp"];

	uint8_t rtcpDetectionInterval = (uint8_t) GetApplication()->GetConfiguration()[CONF_APPLICATION_RTCPDETECTIONINTERVAL];
	if (pFrom->GetCustomParameters().HasKeyChain(_V_NUMERIC, true, 1, CONF_APPLICATION_RTCPDETECTIONINTERVAL))
		rtcpDetectionInterval = (uint8_t) pFrom->GetCustomParameters()[CONF_APPLICATION_RTCPDETECTIONINTERVAL];

	//5. Store the tracks inside the session for later use
	if (audioTrack != V_NULL) {
		audioTrack["isTcp"] = (bool)forceTcp;
		pFrom->GetCustomParameters()["pendingTracks"][(uint32_t) SDP_TRACK_GLOBAL_INDEX(audioTrack)] = audioTrack;
	}
	if (videoTrack != V_NULL) {
		videoTrack["isTcp"] = (bool)forceTcp;
		pFrom->GetCustomParameters()["pendingTracks"][(uint32_t) SDP_TRACK_GLOBAL_INDEX(videoTrack)] = videoTrack;
	}

	//6. Save the streamName
	string streamName = sdp.GetStreamName();
	if (streamName == "") {
		streamName = format("rtsp_stream_%u", pFrom->GetId());
	}
	pFrom->GetCustomParameters()["sdpStreamName"] = streamName;

	//7. Save the bandwidth hint
	pFrom->GetCustomParameters()["sdpBandwidthHint"] = (uint32_t) sdp.GetTotalBandwidth();

	//8. Get the inbound connectivity
	InboundConnectivity *pInboundConnectivity = pFrom->GetInboundConnectivity(
			streamName,
			sdp.GetTotalBandwidth(),
			rtcpDetectionInterval);
	if (pInboundConnectivity == NULL) {
		FATAL("Unable to create inbound connectivity");
		return false;
	}

	//9. Start sending the setup commands on the pending tracks;
	return SendSetupTrackMessages(pFrom);
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse200Setup(
		RTSPProtocol *pFrom, Variant &requestHeaders, string &requestContent,
		Variant &responseHeaders, string &responseContent) {
	if (pFrom->GetCustomParameters()["connectionType"] == "pull") {
		if ((uint32_t) responseHeaders[RTSP_FIRST_LINE][RTSP_STATUS_CODE] != 200) {
			FATAL("request %s failed with response %s",
					STR(requestHeaders.ToString()),
					STR(responseHeaders.ToString()));
			return false;
		}

		if (pFrom->GetCustomParameters()["pendingTracks"].MapSize() != 0) {
			return SendSetupTrackMessages(pFrom);
		}

		//2. Do the play command
		string uri = (string) pFrom->GetCustomParameters()["uri"]["fullUri"];
		int64_t rangeStart = (int64_t) pFrom->GetCustomParameters()["customParameters"]["externalStreamConfig"]["rangeStart"];
		int64_t rangeEnd = (int64_t) pFrom->GetCustomParameters()["customParameters"]["externalStreamConfig"]["rangeEnd"];
		if ((rangeStart == rangeEnd)&&(rangeStart == 0)) {
			rangeStart = -2;
			rangeEnd = -1;
		}
		string range = format("npt=%s-%s",
				(rangeStart < 0) ? "now" : STR(format("%"PRId64, rangeStart)),
				(rangeEnd < 0) ? "" : STR(format("%"PRId64, rangeEnd)));

		//3. prepare the play command
		pFrom->PushRequestFirstLine(RTSP_METHOD_PLAY, uri, RTSP_VERSION_1_0);
		pFrom->PushRequestHeader(RTSP_HEADERS_RANGE, range);

		return pFrom->SendRequestMessage();
	} else {
		if (!responseHeaders[RTSP_HEADERS].HasKey(RTSP_HEADERS_TRANSPORT, false)) {
			FATAL("RTSP %s request doesn't have %s header line",
					RTSP_METHOD_SETUP,
					RTSP_HEADERS_TRANSPORT);
			return false;
		}

		//3. get the transport header line
		string raw = responseHeaders[RTSP_HEADERS].GetValue(RTSP_HEADERS_TRANSPORT, false);
		Variant transport;
		if (!SDP::ParseTransportLinePart(raw, transport)) {
			FATAL("Unable to parse transport line %s", STR(raw));
			return false;
		}
		bool forceTcp = false;
		if (transport.HasKey("server_port")
				&& (transport.HasKey("rtp/avp/udp") || transport.HasKey("rtp/avp"))) {
			forceTcp = false;
		} else if (transport.HasKey("interleaved") && transport.HasKey("rtp/avp/tcp")) {
			forceTcp = true;
		} else {
			FATAL("Invalid transport line: %s", STR(transport.ToString()));
			return false;
		}

		if (forceTcp != (bool)pFrom->GetCustomParameters().GetValue("forceTcp", false)) {
			FATAL("Invalid transport line: %s", STR(transport.ToString()));
			return false;
		}

		OutboundConnectivity *pConnectivity = GetOutboundConnectivity(pFrom, forceTcp);
		if (pConnectivity == NULL) {
			FATAL("Unable to get outbound connectivity");
			return false;
		}

		Variant &params = pFrom->GetCustomParameters();
		if (params["lastSetup"] == "audio") {
			params["audioTransport"] = transport;
		} else {
			params["videoTransport"] = transport;
		}


		Variant &variantUri = params["uri"];

		string trackId = "";
		bool isAudio = false;
		if (params.HasKey("audioTrackId")) {
			trackId = (string) params["audioTrackId"];
			params.RemoveKey("audioTrackId");
			params["lastSetup"] = "audio";
			isAudio = true;
			pConnectivity->HasAudio(true);
		} else {
			if (params.HasKey("videoTrackId")) {
				trackId = (string) params["videoTrackId"];
				params.RemoveKey("videoTrackId");
				params["lastSetup"] = "video";
				pConnectivity->HasVideo(true);
			}
		}

		if (trackId != "") {
			string uri = (string) variantUri["fullUri"] + "/trackID=" + trackId;
			pFrom->PushRequestFirstLine(RTSP_METHOD_SETUP, uri, RTSP_VERSION_1_0);
			string transport = "";
			if (forceTcp) {
				transport = format("RTP/AVP/TCP;unicast;interleaved=%s;mode=record",
						isAudio ? STR(pConnectivity->GetAudioChannels())
						: STR(pConnectivity->GetVideoChannels()));
			} else {
				transport = format("RTP/AVP;unicast;client_port=%s;mode=record",
						isAudio ? STR(pConnectivity->GetAudioPorts())
						: STR(pConnectivity->GetVideoPorts()));
			}
			pFrom->PushRequestHeader(RTSP_HEADERS_TRANSPORT, transport);
			return pFrom->SendRequestMessage();
		} else {
			pFrom->PushRequestFirstLine(RTSP_METHOD_RECORD,
					(string) variantUri["fullUri"],
					RTSP_VERSION_1_0);
			return pFrom->SendRequestMessage();
		}
	}
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse200Play(
		RTSPProtocol *pFrom, Variant &requestHeaders, string &requestContent,
		Variant &responseHeaders, string &responseContent) {
	//1. Get the inbound connectivity
	InboundConnectivity *pConnectivity = pFrom->GetInboundConnectivity();
	if (pConnectivity == NULL) {
		FATAL("Unable to get inbound connectivity");
		return false;
	}

	//2. Create the stream
	if (!pConnectivity->Initialize()) {
		FATAL("Unable to initialize inbound connectivity");
		return false;
	}

	//3. Enable keep alive
	if (!pFrom->EnableKeepAlive(10, pFrom->GetCustomParameters()["uri"]["fullUri"])) {
		FATAL("Unable to enale RTSP keep-alive");
		return false;
	}

	pFrom->EnableTearDown();

	return true;
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse200Announce(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	bool forceTcp = (bool)pFrom->GetCustomParameters().GetValue("forceTcp", false);
	OutboundConnectivity *pConnectivity = GetOutboundConnectivity(pFrom, forceTcp);
	if (pConnectivity == NULL) {
		FATAL("Unable to get outbound connectivity");
		return false;
	}

	Variant &params = pFrom->GetCustomParameters();
	string trackId = "";
	bool isAudio = false;
	if (params.HasKey("audioTrackId")) {
		trackId = (string) params["audioTrackId"];
		params.RemoveKey("audioTrackId");
		params["lastSetup"] = "audio";
		isAudio = true;
		pConnectivity->HasAudio(true);
	} else {
		if (params.HasKey("videoTrackId")) {
			trackId = (string) params["videoTrackId"];
			params.RemoveKey("videoTrackId");
			params["lastSetup"] = "video";
			isAudio = false;
			pConnectivity->HasVideo(true);
		}
	}

	if (trackId != "") {
		Variant &variantUri = params["uri"];
		string uri = (string) variantUri["fullUri"] + "/trackID=" + trackId;
		pFrom->PushRequestFirstLine(RTSP_METHOD_SETUP, uri, RTSP_VERSION_1_0);
		string transport = "";
		if (forceTcp) {
			transport = format("RTP/AVP/TCP;unicast;interleaved=%s;mode=record",
					isAudio ? STR(pConnectivity->GetAudioChannels())
					: STR(pConnectivity->GetVideoChannels()));
		} else {
			transport = format("RTP/AVP;unicast;client_port=%s;mode=record",
					isAudio ? STR(pConnectivity->GetAudioPorts())
					: STR(pConnectivity->GetVideoPorts()));
		}
		pFrom->PushRequestHeader(RTSP_HEADERS_TRANSPORT, transport);
		return pFrom->SendRequestMessage();
	} else {
		FATAL("Bogus RTSP connection");
		pFrom->EnqueueForDelete();
		return false;
	}
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse200Record(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	bool forceTcp = (bool)pFrom->GetCustomParameters().GetValue("forceTcp", false);

	OutboundConnectivity *pConnectivity = GetOutboundConnectivity(pFrom, forceTcp);
	if (pConnectivity == NULL) {
		FATAL("Unable to get outbound connectivity");
		return false;
	}

	bool result = false;

	Variant &params = pFrom->GetCustomParameters();
	if (params.HasKey("audioTransport")) {
		if (forceTcp) {
			if (!pConnectivity->RegisterTCPAudioClient(pFrom->GetId(),
					(uint8_t) params["audioTransport"]["interleaved"]["data"],
					(uint8_t) params["audioTransport"]["interleaved"]["rtcp"])) {
				FATAL("Unable to register audio stream");
				return false;
			}
		} else {
			sockaddr_in dataAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddress();
			dataAddress.sin_port = EHTONS((uint16_t) params["audioTransport"]["server_port"]["data"]);
			sockaddr_in rtcpAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddress();
			rtcpAddress.sin_port = EHTONS((uint16_t) params["audioTransport"]["server_port"]["rtcp"]);
			if (!pConnectivity->RegisterUDPAudioClient(pFrom->GetId(),
					dataAddress, rtcpAddress)) {
				FATAL("Unable to register audio stream");
				return false;
			}
		}
		result |= true;
	}

	if (params.HasKey("videoTransport")) {
		if (forceTcp) {
			if (!pConnectivity->RegisterTCPVideoClient(pFrom->GetId(),
					(uint8_t) params["videoTransport"]["interleaved"]["data"],
					(uint8_t) params["videoTransport"]["interleaved"]["rtcp"])) {
				FATAL("Unable to register audio stream");
				return false;
			}
		} else {
			sockaddr_in dataAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddress();
			dataAddress.sin_port = EHTONS((uint16_t) params["videoTransport"]["server_port"]["data"]);
			sockaddr_in rtcpAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddress();
			rtcpAddress.sin_port = EHTONS((uint16_t) params["videoTransport"]["server_port"]["rtcp"]);
			if (!pConnectivity->RegisterUDPVideoClient(pFrom->GetId(),
					dataAddress, rtcpAddress)) {
				FATAL("Unable to register audio stream");
				return false;
			}
			result |= true;
		}
	}

	if (result)
		pFrom->EnableTearDown();

	EnableDisableOutput(pFrom, true);

	return result;
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse404Play(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	FATAL("PLAY: Resource not found: %s", STR(requestHeaders[RTSP_FIRST_LINE][RTSP_URL]));
	return false;
}

bool BaseRTSPAppProtocolHandler::HandleRTSPResponse404Describe(RTSPProtocol *pFrom, Variant &requestHeaders,
		string &requestContent, Variant &responseHeaders,
		string &responseContent) {
	FATAL("DESCRIBE: Resource not found: %s", STR(requestHeaders[RTSP_FIRST_LINE][RTSP_URL]));
	return false;
}

bool BaseRTSPAppProtocolHandler::TriggerPlayOrAnnounce(RTSPProtocol *pFrom) {
	//1. Save the URL in the custom parameters
	string uri = (string) pFrom->GetCustomParameters()["uri"]["fullUri"];

	//2. prepare the options command
	pFrom->PushRequestFirstLine(RTSP_METHOD_OPTIONS, uri, RTSP_VERSION_1_0);

	//3. Send it
	if (!pFrom->SendRequestMessage()) {
		FATAL("Unable to send the %s request", RTSP_METHOD_OPTIONS);
		return false;
	}

	//4. Done
	return true;
}

bool BaseRTSPAppProtocolHandler::NeedAuthentication(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	//by default, authenticate everything except OPTIONS
	string method = requestHeaders[RTSP_FIRST_LINE][RTSP_METHOD];
	return (method != RTSP_METHOD_OPTIONS);
}

string BaseRTSPAppProtocolHandler::GetAuthenticationRealm(RTSPProtocol *pFrom,
		Variant &requestHeaders, string &requestContent) {
	//by default, just return the first realm
	if (_realms.MapSize() != 0)
		return MAP_KEY(_realms.begin());
	return "";
}

void BaseRTSPAppProtocolHandler::ComputeRTPInfoHeader(RTSPProtocol *pFrom,
		OutboundConnectivity *pOutboundConnectivity, double start) {
	Variant &params = pFrom->GetCustomParameters();
	string rtpInfo;

	FOR_MAP(params["rtpInfo"], string, Variant, i) {
		uint32_t rtpTime = (uint32_t) (((uint64_t) (start * (double) MAP_VAL(i)["frequency"]))&0xffffffff);
		uint16_t sequence = (MAP_KEY(i) == "audio") ?
				(uint16_t) pOutboundConnectivity->GetLastAudioSequence()
				: (uint16_t) pOutboundConnectivity->GetLastVideoSequence();
		if (rtpInfo != "")
			rtpInfo += ",";
		rtpInfo += format("url=%s;seq=%"PRIu16";rtptime=%"PRIu32,
				STR(MAP_VAL(i)["url"]),
				sequence,
				rtpTime
				);
	}

	if (rtpInfo != "")
		pFrom->PushResponseHeader(RTSP_HEADERS_RTP_INFO, rtpInfo);
}

void BaseRTSPAppProtocolHandler::ParseRange(string raw, double &start, double &end) {
	start = 0;
	end = -1;

	//( *)npt( *)=( *)<rest>
	//rest: now( *)-( *)
	//rest: <number>( *)-( *)
	//rest: <number>( *)-( *)<number>( *)

	trim(raw);
	if (raw.find("npt") != 0)
		return;
	raw = raw.substr(3);
	trim(raw);
	if ((raw.size() == 0) || (raw[0] != '='))
		return;
	raw = raw.substr(1);
	trim(raw);

	if ((raw == "") || (raw.find("now") == 0))
		return;

	string::size_type dashPos = raw.find("-");
	if ((dashPos == string::npos) || (dashPos == 0))
		return;

	start = ParseNPT(raw.substr(0, dashPos));
	end = ParseNPT(raw.substr(dashPos + 1));

	if (start < 0)
		start = 0;
}

double BaseRTSPAppProtocolHandler::ParseNPT(string raw) {
	trim(raw);

	if ((raw == "") || (raw == "now"))
		return -1;

	if (raw.find(":") == string::npos) {
		return strtod(STR(raw), NULL);
	} else {
		string::size_type firstColon = raw.find(":", 0);
		string::size_type secondColon = raw.find(":", firstColon + 1);
		string::size_type dot = raw.find(".", secondColon + 1);
		if ((firstColon == string::npos)
				|| (secondColon == string::npos)
				|| (firstColon == secondColon))
			return -1;
		uint32_t hours = (uint32_t) atoi(STR(raw.substr(0, firstColon)));
		uint32_t minutes = (uint32_t) atoi(STR(raw.substr(firstColon + 1)));
		uint32_t seconds = (uint32_t) atoi(STR(raw.substr(secondColon + 1)));
		uint32_t ms = 0;
		if (dot != string::npos) {
			ms = (uint32_t) atoi(STR(raw.substr(dot + 1)));
		}
		return (double) (hours * 3600 + minutes * 60 + seconds)+(double) ms / 1000.0;
	}
}

string BaseRTSPAppProtocolHandler::GetStreamName(RTSPProtocol *pFrom) {
	if (pFrom->GetCustomParameters().HasKey("streamName"))
		return (string) pFrom->GetCustomParameters()["streamName"];
	pFrom->GetCustomParameters()["streamName"] = "";
	return "";
}

bool BaseRTSPAppProtocolHandler::AnalyzeUri(RTSPProtocol *pFrom, string rawUri) {
	/*
	 * We have the following combinations for the structure of the URI used
	 * to access a stream via RTSP
	 * 1. /vod/x
	 * 2. /vodts/x
	 * 3. /tsvod/x
	 * 4. /ts/x
	 * 5. /vod/x?forceTs
	 * 6. /x?forceTs
	 *
	 * x - stream name
	 * vod - request for a file
	 * ts - request for a TS transport
	 * ?forceTs - same as ts but needed for backward compatibility
	 *
	 * vod,ts and forceTs particles should be treated case-insensitive. x, since
	 * is the stream name, must be treated as case-sensitive
	 */

	URI uri;
	if (!URI::FromString(rawUri, false, uri)) {
		FATAL("Invalid URI: %s", STR(rawUri));
		return false;
	}

	bool isVod = false;
	bool isTs = false;
	string streamName;

	string fullDocumentPathCaseInsensitive = lowerCase(uri.fullDocumentPath());
	string fullDocumentPathCaseSensitive = uri.fullDocumentPath();
	string::size_type fullDocumentPathSize = fullDocumentPathCaseInsensitive.length();

	if ((fullDocumentPathSize >= 6)
			&& (fullDocumentPathCaseInsensitive.substr(0, 5) == "/vod/")) {
		//case 1
		isVod = true;
		streamName = fullDocumentPathCaseSensitive.substr(5);
	} else if ((fullDocumentPathSize >= 8)
			&& ((fullDocumentPathCaseInsensitive.substr(0, 7) == "/vodts/")
			|| (fullDocumentPathCaseInsensitive.substr(0, 7) == "/tsvod/"))
			) {
		//case 2 and 3
		isVod = true;
		isTs = true;
		streamName = fullDocumentPathCaseSensitive.substr(7);
	} else if ((fullDocumentPathSize >= 5)
			&& (fullDocumentPathCaseInsensitive.substr(0, 4) == "/ts/")) {
		//case 4
		isTs = true;
		streamName = fullDocumentPathCaseSensitive.substr(4);
	} else if (fullDocumentPathSize >= 2) {
		streamName = fullDocumentPathCaseSensitive.substr(1);
	}

	trim(streamName);
	if (streamName == "") {
		FATAL("Stream name is empty");
		return false;
	}

	// For cases 5 and 6
	if (uri.parameters().HasKey("forceTs", false)) {
		isTs = true;
	}

	Variant &params = pFrom->GetCustomParameters();
	params["isVod"] = (bool)isVod;
	params["isTs"] = (bool)isTs;
	params["streamName"] = streamName;

	BaseInStream *pInStream = GetInboundStream(streamName, pFrom);
	if (pInStream == NULL) {
		FATAL("Stream named %s not found", STR(streamName));
		//FINEST("params:\n%s", STR(params.ToString()));
		return false;
	}

	params["isTs"] = (bool)(isTs);


	//FINEST("params:\n%s", STR(params.ToString()));
	return true;
}

OutboundConnectivity *BaseRTSPAppProtocolHandler::GetOutboundConnectivity(
		RTSPProtocol *pFrom, bool forceTcp) {
	//1. Get the inbound stream
	BaseInNetStream *pInNetStream =
			(BaseInNetStream *) GetApplication()->GetStreamsManager()->FindByUniqueId(
			pFrom->GetCustomParameters()["streamId"]);
	if (pInNetStream == NULL) {
		FATAL("Inbound stream %u not found",
				(uint32_t) pFrom->GetCustomParameters()["streamId"]);
		return NULL;
	}

	//2. Get the outbound connectivity
	OutboundConnectivity *pOutboundConnectivity = pFrom->GetOutboundConnectivity(
			pInNetStream, forceTcp);
	if (pOutboundConnectivity == NULL) {
		FATAL("Unable to get the outbound connectivity");
		return NULL;
	}

	return pOutboundConnectivity;
}

BaseInStream *BaseRTSPAppProtocolHandler::GetInboundStream(string streamName,
		RTSPProtocol *pFrom) {
	//1. get all the inbound network streams which begins with streamName
	map<uint32_t, BaseStream *> streams = GetApplication()->GetStreamsManager()
			->FindByTypeByName(ST_IN_NET, streamName, true, false);
	if (streams.size() == 0) {
		return NULL;
	}

	//2. Get the fisrt value and see if it is compatible
	BaseInNetStream * pResult = (BaseInNetStream *) MAP_VAL(streams.begin());
	if (!pResult->IsCompatibleWithType(ST_OUT_NET_RTP)) {
		FATAL("The stream %s is not compatible with stream type %s",
				STR(streamName), STR(tagToString(ST_OUT_NET_RTP)));
		return NULL;
	}

	//2. Done
	return pResult;
}

StreamCapabilities *BaseRTSPAppProtocolHandler::GetInboundStreamCapabilities(
		string streamName, RTSPProtocol *pFrom) {
	BaseInStream *pInboundStream = GetInboundStream(streamName, pFrom);
	if (pInboundStream == NULL) {
		FATAL("Stream %s not found", STR(streamName));
		return NULL;
	}

	return pInboundStream->GetCapabilities();
}

string BaseRTSPAppProtocolHandler::GetAudioTrack(RTSPProtocol *pFrom,
		StreamCapabilities *pCapabilities) {
	string result = "";
	AudioCodecInfoAAC *pInfo = NULL;
	if ((pCapabilities != NULL)
			&& (pCapabilities->GetAudioCodecType() == CODEC_AUDIO_AAC)
			&& ((pInfo = pCapabilities->GetAudioCodec<AudioCodecInfoAAC > ()) != NULL)) {
		if (pFrom->GetCustomParameters().HasKey("videoTrackId")) {
			pFrom->GetCustomParameters()["audioTrackId"] = "2";
		} else {
			pFrom->GetCustomParameters()["audioTrackId"] = "1";
		}
		result += "m=audio 0 RTP/AVP 96\r\n";
		result += "a=recvonly\r\n";
		result += format("a=rtpmap:96 mpeg4-generic/%"PRIu32"/2\r\n",
				pInfo->_samplingRate);
		pFrom->GetCustomParameters()["rtpInfo"]["audio"]["frequency"] = (uint32_t) pInfo->_samplingRate;
		//FINEST("result: %s", STR(result));
		result += "a=control:trackID="
				+ (string) pFrom->GetCustomParameters()["audioTrackId"] + "\r\n";
		//rfc3640-fmtp-explained.txt Chapter 4.1
		result += format("a=fmtp:96 streamtype=5; profile-level-id=15; mode=AAC-hbr; config=%s; SizeLength=13; IndexLength=3; IndexDeltaLength=3;\r\n",
				STR(hex(pInfo->_pCodecBytes, (uint32_t) pInfo->_codecBytesLength)));
	} else {
		pFrom->GetCustomParameters().RemoveKey("audioTrackId");
		WARN("Unsupported audio codec for RTSP output");
	}
	return result;
}

string BaseRTSPAppProtocolHandler::GetVideoTrack(RTSPProtocol *pFrom,
		StreamCapabilities *pCapabilities) {
	string result = "";
	VideoCodecInfoH264 *pInfo = NULL;
	if ((pCapabilities != NULL)
			&& (pCapabilities->GetVideoCodecType() == CODEC_VIDEO_H264)
			&& ((pInfo = pCapabilities->GetVideoCodec<VideoCodecInfoH264 > ()) != NULL)) {
		if (pFrom->GetCustomParameters().HasKey("audioTrackId"))
			pFrom->GetCustomParameters()["videoTrackId"] = "2"; //md5(format("V%u%s",pFrom->GetId(), STR(generateRandomString(4))), true);
		else
			pFrom->GetCustomParameters()["videoTrackId"] = "1"; //md5(format("V%u%s",pFrom->GetId(), STR(generateRandomString(4))), true);
		result += "m=video 0 RTP/AVP 97\r\n";
		result += "a=recvonly\r\n";
		result += "a=control:trackID="
				+ (string) pFrom->GetCustomParameters()["videoTrackId"] + "\r\n";
		result += format("a=rtpmap:97 H264/%"PRIu32"\r\n", pInfo->_samplingRate);
		pFrom->GetCustomParameters()["rtpInfo"]["video"]["frequency"] = (uint32_t) pInfo->_samplingRate;
		result += "a=fmtp:97 profile-level-id=";
		result += hex(pInfo->_pSPS + 1, 3);
		result += "; packetization-mode=1; sprop-parameter-sets=";
		result += b64(pInfo->_pSPS, pInfo->_spsLength) + ",";
		result += b64(pInfo->_pPPS, pInfo->_ppsLength) + "\r\n";
	} else {
		pFrom->GetCustomParameters().RemoveKey("videoTrackId");
		WARN("Unsupported video codec for RTSP output");
	}
	return result;
}

bool BaseRTSPAppProtocolHandler::SendSetupTrackMessages(RTSPProtocol *pFrom) {
	//1. Get the pending tracks
	if (pFrom->GetCustomParameters()["pendingTracks"].MapSize() == 0) {
		WARN("No more tracks");
		return true;
	}

	//2. Get the inbound connectivity
	InboundConnectivity *pConnectivity = pFrom->GetInboundConnectivity();
	if (pConnectivity == NULL) {
		FATAL("Unable to get inbound connectivity");
		return false;
	}

	//3. Get the first pending track
	Variant track = MAP_VAL(pFrom->GetCustomParameters()["pendingTracks"].begin());
	if (track != V_MAP) {
		FATAL("Invalid track");
		return false;
	}

	//4. Add the track to the inbound connectivity
	if (!pConnectivity->AddTrack(track, (bool)track["isAudio"])) {
		FATAL("Unable to add the track to inbound connectivity");
		return false;
	}

	//6. Prepare the SETUP request
	pFrom->PushRequestFirstLine(RTSP_METHOD_SETUP,
			SDP_VIDEO_CONTROL_URI(track), RTSP_VERSION_1_0);
	pFrom->PushRequestHeader(RTSP_HEADERS_TRANSPORT,
			pConnectivity->GetTransportHeaderLine((bool)track["isAudio"], true));

	//7. Remove the track from pending
	pFrom->GetCustomParameters()["pendingTracks"].RemoveKey(
			MAP_KEY(pFrom->GetCustomParameters()["pendingTracks"].begin()));

	//8. Send the request message
	return pFrom->SendRequestMessage();
}

bool BaseRTSPAppProtocolHandler::ParseUsersFile() {
	//1. get the modification date
	double modificationDate = getFileModificationDate(_usersFile);
	if (modificationDate == 0) {
		FATAL("Unable to get last modification date for file %s", STR(_usersFile));
		_realms.Reset();
		return false;
	}

	//2. Do we need to re-parse everything?
	if (modificationDate == _lastUsersFileUpdate)
		return true;

	//3. Reset realms
	_realms.Reset();

	Variant users;
	//4. Read users
	if (!ReadLuaFile(_usersFile, "users", users)) {
		FATAL("Unable to read users file: `%s`", STR(_usersFile));
		_realms.Reset();
		return false;
	}

	FOR_MAP(users, string, Variant, i) {
		if ((VariantType) MAP_VAL(i) != V_STRING) {
			FATAL("Invalid user detected");
			_realms.Reset();
			return false;
		}
	}

	//5. read the realms
	Variant realms;
	if (!ReadLuaFile(_usersFile, "realms", realms)) {
		FATAL("Unable to read users file: `%s`", STR(_usersFile));
		_realms.Reset();
		return false;
	}

	if (realms != V_MAP) {
		FATAL("Invalid users file. Realms section is bogus: `%s`", STR(_usersFile));
		_realms.Reset();
		return false;
	}

	FOR_MAP(realms, string, Variant, i) {
		Variant &realm = MAP_VAL(i);
		if ((!realm.HasKeyChain(V_STRING, true, 1, "name"))
				|| (realm["name"] == "")
				|| (!realm.HasKeyChain(V_STRING, true, 1, "method"))
				|| ((realm["method"] != "Basic") && (realm["method"] != "Digest"))
				|| (!realm.HasKeyChain(V_MAP, true, 1, "users"))
				|| (realm["users"].MapSize() == 0)) {
			FATAL("Invalid users file. Realms section is bogus: `%s`", STR(_usersFile));
			_realms.Reset();
			return false;
		}
		_realms[realm["name"]]["name"] = realm["name"];
		_realms[realm["name"]]["method"] = realm["method"];

		FOR_MAP(realm["users"], string, Variant, i) {
			if (!users.HasKey(MAP_VAL(i))) {
				FATAL("Invalid users file. Realms section is bogus: `%s`", STR(_usersFile));
				_realms.Reset();
				return false;
			}
			_realms[realm["name"]]["users"][MAP_VAL(i)] = users[MAP_VAL(i)];
		}
	}

	_lastUsersFileUpdate = modificationDate;
	return true;
}

bool BaseRTSPAppProtocolHandler::SendAuthenticationChallenge(RTSPProtocol *pFrom,
		Variant &realm) {
	//10. Ok, the user doesn't know that this needs authentication. We
	//will respond back with a nice 401. Generate the line first
	string wwwAuthenticate = HTTPAuthHelper::GetWWWAuthenticateHeader(
			realm["method"],
			realm["name"]);

	//12. Save the nonce for later validation when new requests are coming in again
	pFrom->GetCustomParameters()["wwwAuthenticate"] = wwwAuthenticate;

	//13. send the response
	pFrom->PushResponseFirstLine(RTSP_VERSION_1_0, 401, "Unauthorized");
	pFrom->PushResponseHeader(HTTP_HEADERS_WWWAUTHENTICATE, wwwAuthenticate);
	return pFrom->SendResponseMessage();
}

string BaseRTSPAppProtocolHandler::ComputeSDP(RTSPProtocol *pFrom,
		string localStreamName, string targetStreamName, bool isAnnounce) {
	string nearAddress = "0.0.0.0";
	string farAddress = "0.0.0.0";
	if ((pFrom->GetIOHandler() != NULL)
			&& (pFrom->GetIOHandler()->GetType() == IOHT_TCP_CARRIER)) {
		nearAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetNearEndpointAddressIp();
		farAddress = ((TCPCarrier *) pFrom->GetIOHandler())->GetFarEndpointAddressIp();
	}

	if (targetStreamName == "")
		targetStreamName = localStreamName;

	//3. Prepare the body of the response
	string result = "";
	result += "v=0\r\n";
	result += format("o=- %"PRIu32" 0 IN IP4 %s\r\n", pFrom->GetId(), STR(nearAddress));
	result += "s=" + targetStreamName + "\r\n";
	result += "u="BRANDING_WEB"\r\n";
	result += "e="BRANDING_EMAIL"\r\n";
	result += "c=IN IP4 " + (isAnnounce ? farAddress : nearAddress) + "\r\n";
	result += "t=0 0\r\n";
	result += "a=recvonly\r\n";
	result += "a=control:*\r\n";
	result += "a=range:npt=now-\r\n";

	StreamCapabilities *pCapabilities = GetInboundStreamCapabilities(
			localStreamName, pFrom);
	if (pCapabilities == NULL) {
		FATAL("Inbound stream %s not found", STR(localStreamName));
		return "";
	}
	string audioTrack = GetAudioTrack(pFrom, pCapabilities);
	string videoTrack = GetVideoTrack(pFrom, pCapabilities);
	if ((audioTrack == "") && (videoTrack == "")) {
		return "";
	}

	result += audioTrack + videoTrack;

	//FINEST("result:\n%s", STR(result));
	return result;
}

void BaseRTSPAppProtocolHandler::EnableDisableOutput(RTSPProtocol *pFrom, bool value) {
	bool forceTcp = (bool)pFrom->GetCustomParameters().GetValue("forceTcp", false);
	OutboundConnectivity *pConnectivity = GetOutboundConnectivity(pFrom, forceTcp);
	if (pConnectivity != NULL)
		pConnectivity->Enable(value);
}

#endif /* HAS_PROTOCOL_RTP */
