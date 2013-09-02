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
#include "application/baseclientapplication.h"
#include "protocols/protocolmanager.h"
#include "application/baseappprotocolhandler.h"
#include "protocols/baseprotocol.h"
#include "streaming/basestream.h"
#include "application/clientapplicationmanager.h"
#include "streaming/streamstypes.h"
#include "mediaformats/readers/streammetadataresolver.h"

uint32_t BaseClientApplication::_idGenerator = 0;

BaseClientApplication::BaseClientApplication(Variant &configuration)
: _streamsManager(this) {
	_id = ++_idGenerator;
	_configuration = configuration;
	_name = (string) configuration[CONF_APPLICATION_NAME];
	if (configuration.HasKeyChain(V_MAP, false, 1, CONF_APPLICATION_ALIASES)) {

		FOR_MAP((configuration[CONF_APPLICATION_ALIASES]), string, Variant, i) {
			ADD_VECTOR_END(_aliases, MAP_VAL(i));
		}
	}
	_isDefault = false;
	if (configuration.HasKeyChain(V_BOOL, false, 1, CONF_APPLICATION_DEFAULT))
		_isDefault = (bool)configuration[CONF_APPLICATION_DEFAULT];
	_hasStreamAliases = false;
	if (configuration.HasKeyChain(V_BOOL, false, 1, CONF_APPLICATION_HAS_STREAM_ALIASES))
		_hasStreamAliases = (bool)configuration[CONF_APPLICATION_HAS_STREAM_ALIASES];
	_pStreamMetadataResolver = new StreamMetadataResolver();
}

BaseClientApplication::~BaseClientApplication() {
	if (_pStreamMetadataResolver != NULL) {
		delete _pStreamMetadataResolver;
		_pStreamMetadataResolver = NULL;
	}
}

uint32_t BaseClientApplication::GetId() {
	return _id;
}

string BaseClientApplication::GetName() {
	return _name;
}

Variant &BaseClientApplication::GetConfiguration() {
	return _configuration;
}

vector<string> BaseClientApplication::GetAliases() {
	return _aliases;
}

bool BaseClientApplication::IsDefault() {
	return _isDefault;
}

StreamsManager *BaseClientApplication::GetStreamsManager() {
	return &_streamsManager;
}

StreamMetadataResolver *BaseClientApplication::GetStreamMetadataResolver() {
	return _pStreamMetadataResolver;
}

bool BaseClientApplication::Initialize() {
	if (_configuration.HasKeyChain(V_STRING, false, 1, CONF_APPLICATION_MEDIAFOLDER)) {
		WARN(CONF_APPLICATION_MEDIAFOLDER" is obsolete. Please use "CONF_APPLICATION_MEDIASTORAGE);
		if (!_configuration.HasKeyChain(V_MAP, false, 1, CONF_APPLICATION_MEDIASTORAGE)) {
			_configuration[CONF_APPLICATION_MEDIASTORAGE] = Variant();
			_configuration[CONF_APPLICATION_MEDIASTORAGE].IsArray(false);
		}
		_configuration.GetValue(CONF_APPLICATION_MEDIASTORAGE, false)["__obsolete__mediaFolder"][CONF_APPLICATION_MEDIAFOLDER] =
				_configuration.GetValue(CONF_APPLICATION_MEDIAFOLDER, false);
	}
	if (_configuration.HasKeyChain(V_MAP, false, 1, CONF_APPLICATION_MEDIASTORAGE)) {
		if (!_pStreamMetadataResolver->Initialize(_configuration.GetValue(CONF_APPLICATION_MEDIASTORAGE, false))) {
			FATAL("Unable to initialize stream metadata resolver");
			return false;
		}
	}

	return true;
}

bool BaseClientApplication::ActivateAcceptors(vector<IOHandler *> &acceptors) {
	for (uint32_t i = 0; i < acceptors.size(); i++) {
		if (!ActivateAcceptor(acceptors[i])) {
			FATAL("Unable to activate acceptor");
			return false;
		}
	}
	return true;
}

bool BaseClientApplication::ActivateAcceptor(IOHandler *pIOHandler) {
	switch (pIOHandler->GetType()) {
		case IOHT_ACCEPTOR:
		{
			TCPAcceptor *pAcceptor = (TCPAcceptor *) pIOHandler;
			pAcceptor->SetApplication(this);
			return pAcceptor->StartAccept();
		}
		case IOHT_UDP_CARRIER:
		{
			UDPCarrier *pUDPCarrier = (UDPCarrier *) pIOHandler;
			pUDPCarrier->GetProtocol()->GetNearEndpoint()->SetApplication(this);
			return pUDPCarrier->StartAccept();
		}
		default:
		{
			FATAL("Invalid acceptor type");
			return false;
		}
	}
}

string BaseClientApplication::GetServicesInfo() {
	map<uint32_t, IOHandler *> handlers = IOHandlerManager::GetActiveHandlers();
	string result = "";

	FOR_MAP(handlers, uint32_t, IOHandler *, i) {
		result += GetServiceInfo(MAP_VAL(i));
	}
	return result;
}

bool BaseClientApplication::AcceptTCPConnection(TCPAcceptor *pTCPAcceptor) {
	return pTCPAcceptor->Accept();
}

void BaseClientApplication::RegisterAppProtocolHandler(uint64_t protocolType,
		BaseAppProtocolHandler *pAppProtocolHandler) {
	if (MAP_HAS1(_protocolsHandlers, protocolType))
		ASSERT("Invalid protocol handler type. Already registered");
	_protocolsHandlers[protocolType] = pAppProtocolHandler;
	pAppProtocolHandler->SetApplication(this);
}

void BaseClientApplication::UnRegisterAppProtocolHandler(uint64_t protocolType) {
	if (MAP_HAS1(_protocolsHandlers, protocolType))
		_protocolsHandlers[protocolType]->SetApplication(NULL);
	_protocolsHandlers.erase(protocolType);
}

bool BaseClientApplication::StreamNameAvailable(string streamName,
		BaseProtocol *pProtocol) {
	if (MAP_HAS1(_streamAliases, streamName))
		return false;
	return _streamsManager.StreamNameAvailable(streamName);
}

BaseAppProtocolHandler *BaseClientApplication::GetProtocolHandler(
		BaseProtocol *pProtocol) {
	if (pProtocol == NULL)
		return NULL;
	return GetProtocolHandler(pProtocol->GetType());
}

BaseAppProtocolHandler *BaseClientApplication::GetProtocolHandler(uint64_t protocolType) {
	if (!MAP_HAS1(_protocolsHandlers, protocolType)) {
		WARN("Protocol handler not activated for protocol type %s in application %s",
				STR(tagToString(protocolType)), STR(_name));
		return NULL;
	}
	return _protocolsHandlers[protocolType];
}

bool BaseClientApplication::HasProtocolHandler(uint64_t protocolType) {
	return MAP_HAS1(_protocolsHandlers, protocolType);
}

BaseAppProtocolHandler *BaseClientApplication::GetProtocolHandler(string &scheme) {
	BaseAppProtocolHandler *pResult = NULL;
	if (false) {

	}
#ifdef HAS_PROTOCOL_RTMP
	else if (scheme.find("rtmp") == 0) {
		pResult = GetProtocolHandler(PT_INBOUND_RTMP);
		if (pResult == NULL)
			pResult = GetProtocolHandler(PT_OUTBOUND_RTMP);
	}
#endif /* HAS_PROTOCOL_RTMP */
#ifdef HAS_PROTOCOL_RTP
	else if (scheme == "rtsp") {
		pResult = GetProtocolHandler(PT_RTSP);
	} else if (scheme == "rtp") {
		pResult = GetProtocolHandler(PT_INBOUND_RTP);
	}
#endif /* HAS_PROTOCOL_RTP */
	else {
		WARN("scheme %s not recognized", STR(scheme));
	}
	return pResult;
}

bool BaseClientApplication::OutboundConnectionFailed(Variant &customParameters) {
	WARN("You should override BaseRTMPAppProtocolHandler::OutboundConnectionFailed");
	return false;
}

void BaseClientApplication::RegisterProtocol(BaseProtocol *pProtocol) {
	if (!MAP_HAS1(_protocolsHandlers, pProtocol->GetType()))
		ASSERT("Protocol handler not activated for protocol type %s in application %s",
			STR(tagToString(pProtocol->GetType())),
			STR(_name));
	_protocolsHandlers[pProtocol->GetType()]->RegisterProtocol(pProtocol);
}

void BaseClientApplication::UnRegisterProtocol(BaseProtocol *pProtocol) {
	if (!MAP_HAS1(_protocolsHandlers, pProtocol->GetType()))
		ASSERT("Protocol handler not activated for protocol type %s in application %s",
			STR(tagToString(pProtocol->GetType())), STR(_name));
	_streamsManager.UnRegisterStreams(pProtocol->GetId());
	_protocolsHandlers[pProtocol->GetType()]->UnRegisterProtocol(pProtocol);
	FINEST("Protocol %s unregistered from application: %s", STR(*pProtocol), STR(_name));
}

void BaseClientApplication::SignalStreamRegistered(BaseStream *pStream) {
	if ((pStream != NULL)&&(pStream->GetType() != ST_NEUTRAL_RTMP))
		INFO("Stream %s registered to application `%s`", STR(*pStream), STR(_name));
}

void BaseClientApplication::SignalStreamUnRegistered(BaseStream *pStream) {
	if ((pStream != NULL)&&(pStream->GetType() != ST_NEUTRAL_RTMP))
		INFO("Stream %s unregistered from application `%s`", STR(*pStream), STR(_name));
}

bool BaseClientApplication::PullExternalStreams() {
	//1. Minimal verifications
	if (_configuration["externalStreams"] == V_NULL) {
		return true;
	}

	if (_configuration["externalStreams"] != V_MAP) {
		FATAL("Invalid rtspStreams node");
		return false;
	}

	//2. Loop over the stream definitions and validate duplicated stream names
	Variant streamConfigs;
	streamConfigs.IsArray(false);

	FOR_MAP(_configuration["externalStreams"], string, Variant, i) {
		Variant &temp = MAP_VAL(i);
		if ((!temp.HasKeyChain(V_STRING, false, 1, "localStreamName"))
				|| (temp.GetValue("localStreamName", false) == "")) {
			WARN("External stream configuration is doesn't have localStreamName property invalid:\n%s",
					STR(temp.ToString()));
			continue;
		}
		string localStreamName = (string) temp.GetValue("localStreamName", false);
		if (streamConfigs.HasKey(localStreamName)) {
			WARN("External stream configuration produces duplicated stream names\n%s",
					STR(temp.ToString()));
			continue;
		}
		streamConfigs[localStreamName] = temp;
	}


	//2. Loop over the stream definitions and spawn the streams

	FOR_MAP(streamConfigs, string, Variant, i) {
		Variant &streamConfig = MAP_VAL(i);
		if (!PullExternalStream(streamConfig)) {
			WARN("External stream configuration is invalid:\n%s",
					STR(streamConfig.ToString()));
		}
	}

	//3. Done
	return true;
}

bool BaseClientApplication::PullExternalStream(Variant &streamConfig) {
	//1. Minimal verification
	if (streamConfig["uri"] != V_STRING) {
		FATAL("Invalid uri");
		return false;
	}

	bool resolveHost = (!streamConfig.HasKeyChain(V_STRING, true, 1, "httpProxy"))
			|| (streamConfig["httpProxy"] == "")
			|| (streamConfig["httpProxy"] == "self");

	//2. Split the URI
	URI uri;
	if (!URI::FromString(streamConfig["uri"], resolveHost, uri)) {
		FATAL("Invalid URI: %s", STR(streamConfig["uri"].ToString()));
		return false;
	}
	streamConfig["uri"] = uri;

	//3. Depending on the scheme name, get the curresponding protocol handler
	///TODO: integrate this into protocol factory manager via protocol factories
	string scheme = uri.scheme();
	BaseAppProtocolHandler *pProtocolHandler = GetProtocolHandler(scheme);
	if (pProtocolHandler == NULL) {
		WARN("Unable to find protocol handler for scheme %s in application %s",
				STR(scheme),
				STR(GetName()));
		return false;
	}

	//4. Initiate the stream pulling sequence
	return pProtocolHandler->PullExternalStream(uri, streamConfig);
}

bool BaseClientApplication::PushLocalStream(Variant &streamConfig) {
	//1. Minimal verification
	if (streamConfig["targetUri"] != V_STRING) {
		FATAL("Invalid uri");
		return false;
	}
	if (streamConfig["localStreamName"] != V_STRING) {
		FATAL("Invalid local stream name");
		return false;
	}
	string streamName = (string) streamConfig["localStreamName"];
	trim(streamName);
	if (streamName == "") {
		FATAL("Invalid local stream name");
		return false;
	}
	streamConfig["localStreamName"] = streamName;

	//2. Split the URI
	URI uri;
	if (!URI::FromString(streamConfig["targetUri"], true, uri)) {
		FATAL("Invalid URI: %s", STR(streamConfig["targetUri"].ToString()));
		return false;
	}
	streamConfig["targetUri"] = uri;

	//3. Depending on the scheme name, get the curresponding protocol handler
	///TODO: integrate this into protocol factory manager via protocol factories
	string scheme = uri.scheme();
	BaseAppProtocolHandler *pProtocolHandler = GetProtocolHandler(scheme);
	if (pProtocolHandler == NULL) {
		WARN("Unable to find protocol handler for scheme %s in application %s",
				STR(scheme),
				STR(GetName()));
		return false;
	}

	//4. Initiate the stream pulling sequence
	return pProtocolHandler->PushLocalStream(streamConfig);
}

bool BaseClientApplication::ParseAuthentication() {
	//1. Get the authentication configuration node
	if (!_configuration.HasKeyChain(V_MAP, false, 1, CONF_APPLICATION_AUTH)) {
		if (_configuration.HasKey(CONF_APPLICATION_AUTH, false)) {
			WARN("Authentication node is present for application %s but is empty or invalid", STR(_name));
		}
		return true;
	}
	Variant &auth = _configuration[CONF_APPLICATION_AUTH];

	//2. Cycle over all access schemas

	FOR_MAP(auth, string, Variant, i) {
		//3. get the schema
		string scheme = MAP_KEY(i);

		//4. Get the handler
		BaseAppProtocolHandler *pHandler = GetProtocolHandler(scheme);
		if (pHandler == NULL) {
			WARN("Authentication parsing for app name %s failed. No handler registered for schema %s",
					STR(_name),
					STR(scheme));
			return true;
		}

		//5. Call the handler
		if (!pHandler->ParseAuthenticationNode(MAP_VAL(i), _authSettings[scheme])) {
			FATAL("Authentication parsing for app name %s failed. scheme was %s",
					STR(_name),
					STR(scheme));
			return false;
		}
	}

	return true;
}

void BaseClientApplication::SignalUnLinkingStreams(BaseInStream *pInStream,
		BaseOutStream *pOutStream) {

}

void BaseClientApplication::Shutdown(BaseClientApplication *pApplication) {
	//1. Get the list of all active protocols
	map<uint32_t, BaseProtocol *> protocols = ProtocolManager::GetActiveProtocols();

	//2. enqueue for delete for all protocols bound to pApplication

	FOR_MAP(protocols, uint32_t, BaseProtocol *, i) {
		if ((MAP_VAL(i)->GetApplication() != NULL)
				&& (MAP_VAL(i)->GetApplication()->GetId() == pApplication->GetId())) {
			MAP_VAL(i)->SetApplication(NULL);
			MAP_VAL(i)->EnqueueForDelete();
		}
	}

	//1. Get the list of all active IOHandlers and enqueue for delete for all services bound to pApplication
	map<uint32_t, IOHandler *> handlers = IOHandlerManager::GetActiveHandlers();

	FOR_MAP(handlers, uint32_t, IOHandler *, i) {
		BaseProtocol *pProtocol = MAP_VAL(i)->GetProtocol();
		BaseProtocol *pTemp = pProtocol;
		while (pTemp != NULL) {
			if ((pTemp->GetApplication() != NULL)
					&& (pTemp->GetApplication()->GetId() == pApplication->GetId())) {
				IOHandlerManager::EnqueueForDelete(MAP_VAL(i));
				break;
			}
			pTemp = pTemp->GetNearProtocol();
		}
	}

	handlers = IOHandlerManager::GetActiveHandlers();

	FOR_MAP(handlers, uint32_t, IOHandler *, i) {
		if ((MAP_VAL(i)->GetType() == IOHT_ACCEPTOR)
				&& (((TCPAcceptor *) MAP_VAL(i))->GetApplication() != NULL)) {
			if (((TCPAcceptor *) MAP_VAL(i))->GetApplication()->GetId() == pApplication->GetId())
				IOHandlerManager::EnqueueForDelete(MAP_VAL(i));
		}
	}

	//4. Unregister it
	ClientApplicationManager::UnRegisterApplication(pApplication);

	//5. Delete it
	delete pApplication;
}

string BaseClientApplication::GetStreamNameByAlias(string &streamName, bool remove) {
	string result = "";

	map<string, string>::iterator i = _streamAliases.find(streamName);

	if (i != _streamAliases.end()) {
		result = MAP_VAL(i);
		if (remove) {
			_streamAliases.erase(i);
		}
	} else {
		if (!_hasStreamAliases) {
			result = streamName;
		}
	}

	if (_aliases.size() > 200) {
		WARN("Auto flush aliases: %"PRIu32, (uint32_t) _aliases.size());
		_aliases.clear();
	}

	return result;
}

bool BaseClientApplication::SetStreamAlias(string &streamName, string &streamAlias) {
	if (!_hasStreamAliases) {
		FATAL(CONF_APPLICATION_HAS_STREAM_ALIASES" property was not set up inside the configuration file");
		return false;
	}
	_streamAliases[streamAlias] = streamName;
	return true;
}

bool BaseClientApplication::RemoveStreamAlias(string &streamAlias) {
	if (!_hasStreamAliases)
		return false;
	_streamAliases.erase(streamAlias);
	return true;
}

map<string, string> & BaseClientApplication::GetAllStreamAliases() {
	return _streamAliases;
}

static const char *gStrSettings[] = {
	"",
	"pullSettings",
	"pushSettings"
};

OperationType BaseClientApplication::GetOperationType(BaseProtocol *pProtocol, Variant &streamConfig) {
	streamConfig.Reset();
	if (pProtocol == NULL)
		return OPERATION_TYPE_STANDARD;
	//2. Get connection type
	return GetOperationType(pProtocol->GetCustomParameters(), streamConfig);
}

OperationType BaseClientApplication::GetOperationType(Variant &allParameters, Variant &streamConfig) {
	//1. Reset the streamconfig
	streamConfig.Reset();

	//2. Check the parameters and see if they are present
	if (allParameters != V_MAP)
		return OPERATION_TYPE_STANDARD;
	if (!allParameters.HasKey("customParameters"))
		return OPERATION_TYPE_STANDARD;
	Variant customParameters = allParameters["customParameters"];
	if (customParameters != V_MAP)
		return OPERATION_TYPE_STANDARD;

	//3. Is this a pull?
	if (customParameters.HasKey("externalStreamConfig")) {
		streamConfig = customParameters["externalStreamConfig"];
		string uri = streamConfig["uri"]["fullUriWithAuth"];
		streamConfig["uri"] = uri;
		return OPERATION_TYPE_PULL;
	}

	//4. Is this a push?
	if (customParameters.HasKey("localStreamConfig")) {
		streamConfig = customParameters["localStreamConfig"];
		string uri = streamConfig["targetUri"]["fullUriWithAuth"];
		streamConfig["targetUri"] = uri;
		return OPERATION_TYPE_PUSH;
	}

	//9. This is something else
	return OPERATION_TYPE_STANDARD;
}

void BaseClientApplication::StoreConnectionType(Variant &dest, BaseProtocol *pProtocol) {
	Variant streamConfig;
	OperationType operationType = GetOperationType(pProtocol, streamConfig);
	if ((operationType >= OPERATION_TYPE_PULL) && (operationType <= OPERATION_TYPE_PUSH)) {
		dest[gStrSettings[operationType]] = streamConfig;
	}
	dest["connectionType"] = (uint8_t) operationType;
}

Variant &BaseClientApplication::GetStreamSettings(Variant &src) {
	OperationType operationType;
	if ((src.HasKeyChain(_V_NUMERIC, true, 1, "connectionType"))
			&& ((operationType = (OperationType) ((uint8_t) src["connectionType"])) >= OPERATION_TYPE_PULL)
			&& (operationType <= OPERATION_TYPE_PUSH)
			&& (src.HasKeyChain(V_MAP, true, 1, gStrSettings[operationType]))
			) {
		return src[gStrSettings[operationType]];
	} else {
		return _dummy;
	}
}

string BaseClientApplication::GetServiceInfo(IOHandler *pIOHandler) {
	if ((pIOHandler->GetType() != IOHT_ACCEPTOR)
			&& (pIOHandler->GetType() != IOHT_UDP_CARRIER))
		return "";
	if (pIOHandler->GetType() == IOHT_ACCEPTOR) {
		if ((((TCPAcceptor *) pIOHandler)->GetApplication() == NULL)
				|| (((TCPAcceptor *) pIOHandler)->GetApplication()->GetId() != GetId())) {
			return "";
		}
	} else {
		if ((((UDPCarrier *) pIOHandler)->GetProtocol() == NULL)
				|| (((UDPCarrier *) pIOHandler)->GetProtocol()->GetNearEndpoint()->GetApplication() == NULL)
				|| (((UDPCarrier *) pIOHandler)->GetProtocol()->GetNearEndpoint()->GetApplication()->GetId() != GetId())) {
			return "";
		}
	}
	Variant &params = pIOHandler->GetType() == IOHT_ACCEPTOR ?
			((TCPAcceptor *) pIOHandler)->GetParameters()
			: ((UDPCarrier *) pIOHandler)->GetParameters();
	if (params != V_MAP)
		return "";
	stringstream ss;
	ss << "+---+---------------+-----+-------------------------+-------------------------+" << endl;
	ss << "|";
	ss.width(3);
	ss << (pIOHandler->GetType() == IOHT_ACCEPTOR ? "tcp" : "udp");
	ss << "|";

	ss.width(3 * 4 + 3);
	ss << (string) params[CONF_IP];
	ss << "|";

	ss.width(5);
	ss << (uint16_t) params[CONF_PORT];
	ss << "|";

	ss.width(25);
	ss << (string) params[CONF_PROTOCOL];
	ss << "|";

	ss.width(25);
	ss << GetName();
	ss << "|";

	ss << endl;

	return ss.str();
}
