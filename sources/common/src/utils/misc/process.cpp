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

void Process::GetFinished(vector<pid_t> &pids, bool &noMorePids) {
	GetFinishedProcesses(pids, noMorePids);
	//	FOR_VECTOR(pids, i) {
	//		FINEST("Pid terminated: %"PRIu64, (uint64_t) pids[i]);
	//	}
}

bool Process::Launch(Variant &settings, pid_t &pid) {
	string rawArguments = settings["arguments"];
	//FINEST("BEFORE: `%s`", STR(rawArguments));
	replace(rawArguments, "\\\\", "_#slash#_");
	replace(rawArguments, "\\ ", "_#space#_");
	//FINEST(" AFTER: `%s`", STR(rawArguments));
	vector<string> arguments;
	split(rawArguments, " ", arguments);

	FOR_VECTOR(arguments, i) {
		replace(arguments[i], "_#space#_", " ");
		replace(arguments[i], "_#slash#_", "\\");
	}


	vector<string> envVars;

	FOR_MAP(settings, string, Variant, i) {
		string key = MAP_KEY(i);
		if ((key.size() < 2) || (key[0] != '$'))
			continue;
		ADD_VECTOR_END(envVars, format("%s=%s", STR(key.substr(1)), STR(MAP_VAL(i))));
	}

	string fullBinaryPath = (string) settings["fullBinaryPath"];

	bool result = LaunchProcess(fullBinaryPath, arguments, envVars, pid);
	//	if (result) {
	//		FINEST("Pid for %s created: %"PRIu64, STR(settings["fullBinaryPath"]), (uint64_t) pid);
	//	}
	return result;
}
