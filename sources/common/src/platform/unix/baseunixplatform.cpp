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

#if defined FREEBSD || defined LINUX || defined OSX || defined SOLARIS
#include "common.h"

bool fileExists(const string &path) {
	struct stat temp;
	return stat(path.c_str(), &temp) == 0;
}

bool createFolder(const string &path, bool recursive) {
	string command = format("mkdir %s %s",
			recursive ? "-p" : "",
			path.c_str());
	if (system(command.c_str()) != 0) {
		FATAL("Unable to create folder %s", path.c_str());
		return false;
	}

	return true;
}

bool deleteFolder(const string &path, bool force) {
	if (!force) {
		return deleteFile(path);
	} else {
		string command = format("rm -rf %s", path.c_str());
		if (system(command.c_str()) != 0) {
			FATAL("Unable to delete folder %s", path.c_str());
			return false;
		}
		return true;
	}
}

bool listFolder(const string &path, vector<string> &result, bool normalizeAllPaths,
		bool includeFolders, bool recursive) {
	if (path == "")
		return listFolder(".", result, normalizeAllPaths, includeFolders,
			recursive);
	if (path[path.size() - 1] != PATH_SEPARATOR)
		return listFolder(path + PATH_SEPARATOR, result, normalizeAllPaths,
			includeFolders, recursive);

	DIR *pDir = NULL;
	pDir = opendir(path.c_str());
	if (pDir == NULL) {
		int err = errno;
		FATAL("Unable to open folder: %s (%d) %s", path.c_str(), err, strerror(err));
		return false;
	}

	struct dirent *pDirent = NULL;
	while ((pDirent = readdir(pDir)) != NULL) {
		string entry = pDirent->d_name;
		if ((entry == ".")
				|| (entry == "..")) {
			continue;
		}
		if (normalizeAllPaths) {
			entry = normalizePath(path, entry);
		} else {
			entry = path + entry;
		}
		if (entry == "")
			continue;

		if (pDirent->d_type == DT_UNKNOWN) {
			struct stat temp;
			if (stat(entry.c_str(), &temp) != 0) {
				WARN("Unable to stat entry %s", entry.c_str());
				continue;
			}
			pDirent->d_type = ((temp.st_mode & S_IFDIR) == S_IFDIR) ? DT_DIR : DT_REG;
		}

		switch (pDirent->d_type) {
			case DT_DIR:
			{
				if (includeFolders) {
					ADD_VECTOR_END(result, entry);
				}
				if (recursive) {
					if (!listFolder(entry + PATH_SEPARATOR, result, normalizeAllPaths, includeFolders, recursive)) {
						FATAL("Unable to list folder");
						closedir(pDir);
						return false;
					}
				}
				break;
			}
			case DT_REG:
			{
				ADD_VECTOR_END(result, entry);
				break;
			}
			default:
			{
				WARN("Invalid dir entry detected");
				break;
			}
		}
	}

	closedir(pDir);
	return true;
}

string normalizePath(const string &base, const string &file) {
	if (base == "")
		return normalizePath("./", file);
	if (base[base.size() - 1] != PATH_SEPARATOR)
		return normalizePath(base + PATH_SEPARATOR, file);

	char dummy1[PATH_MAX + 1];
	char dummy2[PATH_MAX + 1];
	char *pBase = realpath(base.c_str(), dummy1);
	char *pFile = realpath((base + file).c_str(), dummy2);

	if (pBase == NULL || pFile == NULL)
		return "";

	return (memcmp(pFile, pBase, strlen(pBase)) == 0) ? (fileExists(pFile) ? pFile : "") : "";
}

string realPath(const string &path) {
	char dummy1[PATH_MAX + 1];
	char *pTemp = realpath(path.c_str(), dummy1);
	return pTemp != NULL ? pTemp : "";
}

bool isAbsolutePath(const string &path) {
	return (bool)((path.size() != 0) && (path[0] == PATH_SEPARATOR));
}

bool setFdCloseOnExec(SOCKET_TYPE fd) {
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		int err = errno;
		FATAL("fcntl failed %d %s", err, strerror(err));
		return false;
	}
	return true;
}

bool setFdNonBlock(SOCKET_TYPE fd) {
	int32_t arg;
	if ((arg = fcntl(fd, F_GETFL, NULL)) < 0) {
		int err = errno;
		FATAL("Unable to get fd flags: (%d) %s", err, strerror(err));
		return false;
	}
	arg |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, arg) < 0) {
		int err = errno;
		FATAL("Unable to set fd flags: (%d) %s", err, strerror(err));
		return false;
	}

	return true;
}

string GetEnvVariable(const char *pEnvVarName) {
	char *pTemp = getenv(pEnvVarName);
	if (pTemp != NULL)
		return pTemp;
	return "";
}

bool setMaxFdCount(uint32_t &current, uint32_t &max) {
	//1. reset stuff
	current = 0;
	max = 0;
	struct rlimit limits;
	memset(&limits, 0, sizeof (limits));

	//2. get the current value
	if (getrlimit(RLIMIT_NOFILE, &limits) != 0) {
		int err = errno;
		FATAL("getrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}
	current = (uint32_t) limits.rlim_cur;
	max = (uint32_t) limits.rlim_max;

	//3. Set the current value to max value
	limits.rlim_cur = (OPEN_MAX < limits.rlim_max) ? OPEN_MAX : limits.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
		int err = errno;
		FATAL("setrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}

	//4. Try to get it back
	memset(&limits, 0, sizeof (limits));
	if (getrlimit(RLIMIT_NOFILE, &limits) != 0) {
		int err = errno;
		FATAL("getrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}
	current = (uint32_t) limits.rlim_cur;
	max = (uint32_t) limits.rlim_max;


	return true;
}

bool enableCoreDumps() {
	struct rlimit limits;
	memset(&limits, 0, sizeof (limits));

	memset(&limits, 0, sizeof (limits));
	if (getrlimit(RLIMIT_CORE, &limits) != 0) {
		int err = errno;
		FATAL("getrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}

	limits.rlim_cur = limits.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &limits) != 0) {
		int err = errno;
		FATAL("setrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}

	memset(&limits, 0, sizeof (limits));
	if (getrlimit(RLIMIT_CORE, &limits) != 0) {
		int err = errno;
		FATAL("getrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}

	return limits.rlim_cur == RLIM_INFINITY;
}

void killProcess(pid_t pid) {
	kill(pid, SIGKILL);
}

int8_t getCPUCount() {
#ifdef ANDROID
	return (int8_t) android_getCpuCount();
#else /* ANDROID */
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif /* ANDROID */
}

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

#ifdef NO_POSIX_SPAWN

bool LaunchProcess(string fullBinaryPath, vector<string> &arguments, vector<string> &envVars, pid_t &pid) {
	FATAL("posix_spawn not supported by this platform");
	return false;
}
#else /* NO_POSIX_SPAWN */
#include <spawn.h>

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
#endif /* NO_POSIX_SPAWN */

bool OpenSysLog(const string name) {
	openlog(STR(name), LOG_PID, 0);
	return true;
}

static const int syslogLevels[] = {
	LOG_ERR, //FATAL
	LOG_ERR, //ERROR
	LOG_WARNING, //WARNING
	LOG_INFO, //INFO
	LOG_DEBUG, //DEBUG
	LOG_DEBUG, //FINE
	LOG_DEBUG //FINEST
};
static const int32_t syslogLevelsCount = sizeof (syslogLevels) / sizeof (int);

void Syslog(int32_t level, const char *message, ...) {
	level = ((level >= 0)&&(level < syslogLevelsCount)) ? syslogLevels[level] : LOG_ERR;
	va_list arguments;
	va_start(arguments, message);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif /* __clang__ */
	vsyslog(level, message, arguments);
#ifdef __clang__
#pragma clang diagnostic pop
#endif /* __clang__ */
	va_end(arguments);
}

void CloseSysLog() {
	closelog();
}

static map<int, SignalFnc> _signalHandlers;

void signalHandler(int sig) {
	if (!MAP_HAS1(_signalHandlers, sig))
		return;
	_signalHandlers[sig]();
}

void installSignal(int sig, SignalFnc pSignalFnc) {
	_signalHandlers[sig] = pSignalFnc;
	struct sigaction action;
	action.sa_handler = signalHandler;
	action.sa_flags = 0;
	if (sigemptyset(&action.sa_mask) != 0) {
		ASSERT("Unable to install the quit signal");
		return;
	}
	if (sigaction(sig, &action, NULL) != 0) {
		ASSERT("Unable to install the quit signal");
		return;
	}
}

void installQuitSignal(SignalFnc pQuitSignalFnc) {
	installSignal(SIGTERM, pQuitSignalFnc);
}

void installConfRereadSignal(SignalFnc pConfRereadSignalFnc) {
	installSignal(SIGHUP, pConfRereadSignalFnc);
}

int64_t getFileSize(int fd) {
	struct stat64 stats;
	if (fstat64(fd, &stats) < 0) {
		int err = errno;
		FATAL("Unable read the size of file. Error was: (%d) %s", err, strerror(err));
		return -1;
	}
	return stats.st_size;
}

#endif /* defined FREEBSD || defined LINUX || defined OSX || defined SOLARIS */
