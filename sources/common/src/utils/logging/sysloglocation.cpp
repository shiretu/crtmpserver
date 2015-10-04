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

SysLogLocation::SysLogLocation(Variant &configuration)
: BaseLogLocation(configuration) {
}

SysLogLocation::~SysLogLocation() {
	CloseSysLog();
}

bool SysLogLocation::Init() {
	if (!BaseLogLocation::Init()) {
		return false;
	}
	if (_configuration.HasKeyChain(V_STRING, false, 1, "name"))
		_loggerName = (string) _configuration.GetValue("name", false);
	trim(_loggerName);
	if (_loggerName == "")
		_loggerName = "ems";
	return OpenSysLog(_loggerName);
}

void SysLogLocation::Log(int32_t level, const char *pFileName,
		uint32_t lineNumber, const char *pFunctionName, string &message) {
	if (_singleLine) {
		replace(message, "\r", "\\r");
		replace(message, "\n", "\\n");
	}
	Syslog(level, "%s [%s:%"PRIu32"]", STR(message), pFileName, lineNumber);
}

void SysLogLocation::SignalFork(uint32_t forkId) {
}
