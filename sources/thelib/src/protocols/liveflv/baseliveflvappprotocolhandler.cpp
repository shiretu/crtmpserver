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


#ifdef HAS_PROTOCOL_LIVEFLV

#include "protocols/liveflv/baseliveflvappprotocolhandler.h"
#include "protocols/baseprotocol.h"
#include "application/baseclientapplication.h"

BaseLiveFLVAppProtocolHandler::BaseLiveFLVAppProtocolHandler(Variant &configuration)
: BaseAppProtocolHandler(configuration) {

}

BaseLiveFLVAppProtocolHandler::~BaseLiveFLVAppProtocolHandler() {
}

bool BaseLiveFLVAppProtocolHandler::ParseAuthenticationNode(Variant &node,
		Variant &result) {
	//1. Users file validation
	string usersFile = node[CONF_APPLICATION_AUTH_USERS_FILE];
	if ((usersFile[0] != '/') && (usersFile[0] != '.')) {
		usersFile = (string) _configuration[CONF_APPLICATION_DIRECTORY] + usersFile;
	}
	if (!fileExists(usersFile)) {
		FATAL("Invalid authentication configuration. Missing users file: %s", STR(usersFile));
		return false;
	}
	_usersFile = usersFile;

	if (!ParseUsersFile()) {
		FATAL("Unable to parse users file %s", STR(usersFile));
		return false;
	}

	return true;
}

void BaseLiveFLVAppProtocolHandler::RegisterProtocol(BaseProtocol *pProtocol) {
	if (MAP_HAS1(_protocols, pProtocol->GetId())) {
		ASSERT("Protocol ID %u already registered", pProtocol->GetId());
		return;
	}
	if (pProtocol->GetType() != PT_INBOUND_LIVE_FLV) {
		ASSERT("This protocol can't be registered here");
		return;
	}
	_protocols[pProtocol->GetId()] = (InboundLiveFLVProtocol *) pProtocol;
	FINEST("protocol %s registered to app %s", STR(*pProtocol), STR(GetApplication()->GetName()));
}

void BaseLiveFLVAppProtocolHandler::UnRegisterProtocol(BaseProtocol *pProtocol) {
	if (!MAP_HAS1(_protocols, pProtocol->GetId())) {
		ASSERT("Protocol ID %u not registered", pProtocol->GetId());
		return;
	}
	if (pProtocol->GetType() != PT_INBOUND_LIVE_FLV) {
		ASSERT("This protocol can't be unregistered here");
		return;
	}
	_protocols.erase(pProtocol->GetId());
	FINEST("protocol %s unregistered from app %s", STR(*pProtocol), STR(GetApplication()->GetName()));
}

bool BaseLiveFLVAppProtocolHandler::ParseUsersFile() {
	if (_usersFile == "") {
		_users.Reset();
		return false;
	}
	//1. get the modification date
	double modificationDate = getFileModificationDate(_usersFile);
	if (modificationDate == 0) {
		FATAL("Unable to get last modification date for file %s", STR(_usersFile));
		_users.Reset();
		return false;
	}

	//2. Do we need to re-parse everything?
	if (modificationDate == _lastUsersFileUpdate)
		return true;

	_users.Reset();

	//4. Read users
	if (!ReadLuaFile(_usersFile, "users", _users)) {
		FATAL("Unable to read users file: `%s`", STR(_usersFile));
		_users.Reset();
		return false;
	}

	FOR_MAP(_users, string, Variant, i) {
		if ((VariantType) MAP_VAL(i) != V_STRING) {
			FATAL("Invalid user detected");
			_users.Reset();
			return false;
		}
	}
	INFO("Parsed Users File %s", STR(_usersFile));
	_lastUsersFileUpdate = modificationDate;
	return true;
}
bool BaseLiveFLVAppProtocolHandler::AuthenticateUser(string username, string password)
{
	ParseUsersFile();
	if (username == "") {
		if (_usersFile!="") {
			FATAL("Client failed to send userName/password in metadata");
			return false;
		}
	} else {
		INFO("Authenticating '%s'", STR(username));
		return _users[username] == password;
	}
	return true;
}

#endif /* HAS_PROTOCOL_LIVEFLV */
