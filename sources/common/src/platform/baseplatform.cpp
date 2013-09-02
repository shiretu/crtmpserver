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

BasePlatform::BasePlatform() {

}

BasePlatform::~BasePlatform() {
}

#if defined FREEBSD || defined LINUX || defined OSX || defined SOLARIS

void GetFinishedProcesses(vector<pid_t> &pids, bool &noMorePids) {
	pids.clear();
	noMorePids = false;
	int statLoc = 0;
	while (true) {
		pid_t pid = waitpid(-1, &statLoc, WNOHANG);
		if (pid < 0) {
			int err = errno;
			if (err != ECHILD)
				WARN("waitpid failed %d %s", err, strerror(err));
			noMorePids = true;
			break;
		}
		if (pid == 0)
			break;
		ADD_VECTOR_END(pids, pid);
	}
}

bool LaunchProcess(string fullBinaryPath, vector<string> &arguments, vector<string> &envVars, pid_t &pid) {
	char **_ppArgs = NULL;
	char **_ppEnv = NULL;

	ADD_VECTOR_BEGIN(arguments, fullBinaryPath);
	_ppArgs = new char*[arguments.size() + 1];
	for (uint32_t i = 0; i < arguments.size(); i++) {
		_ppArgs[i] = new char[arguments[i].size() + 1];
		strcpy(_ppArgs[i], arguments[i].c_str());
	}
	_ppArgs[arguments.size()] = NULL;

	if (envVars.size() > 0) {
		_ppEnv = new char*[envVars.size() + 1];
		for (uint32_t i = 0; i < envVars.size(); i++) {
			_ppEnv[i] = new char[envVars[i].size() + 1];
			strcpy(_ppEnv[i], envVars[i].c_str());
		}
		_ppEnv[envVars.size()] = NULL;
	}

	if (posix_spawn(&pid, STR(fullBinaryPath), NULL, NULL, _ppArgs, _ppEnv) != 0) {
		int err = errno;
		FATAL("posix_spawn failed %d %s", err, strerror(err));
		IOBuffer::ReleaseDoublePointer(&_ppArgs);
		IOBuffer::ReleaseDoublePointer(&_ppEnv);
		return false;
	}

	IOBuffer::ReleaseDoublePointer(&_ppArgs);
	IOBuffer::ReleaseDoublePointer(&_ppEnv);
	return true;
}

bool setFdCloseOnExec(int fd) {
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		int err = errno;
		FATAL("fcntl failed %d %s", err, strerror(err));
		return false;
	}
	return true;
}

void killProcess(pid_t pid) {
	kill(pid, SIGKILL);
}

#endif /* defined FREEBSD || defined LINUX || defined OSX || defined SOLARIS */
