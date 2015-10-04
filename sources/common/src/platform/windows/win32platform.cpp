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

#ifdef WIN32

#include "platform/platform.h"
#include "utils/logging/logging.h"
#include <Strsafe.h>
#include <stdio.h>
#include <TlHelp32.h>
#include <io.h>

static map<uint32_t, SignalFnc> _signalHandlers;
static vector<pid_t> _pids;

string GetEnvVariable(const char *pEnvVarName) {
	string result = "";
	size_t len = 0;
	char *pEnv = NULL;
	_dupenv_s(&pEnv, &len, pEnvVarName);
	if (pEnv != NULL) {
		result = pEnv;
		free(pEnv);
		pEnv = NULL;
	}
	return result;
}

int vasprintf(char **strp, const char *fmt, va_list ap, int size) {
	*strp = (char *) malloc(size);
	int result = 0;
	if ((result = vsnprintf(*strp, size, fmt, ap)) == -1) {
		free(*strp);
		return vasprintf(strp, fmt, ap, size + size / 2);
	} else {
		return result;
	}
}

bool fileExists(const string &path) {
	return PathFileExists(path.c_str()) != 0;
}

int8_t getCPUCount() {
	WARN("Windows doesn't support multiple instances");
	return 0;
}

int gettimeofday(struct timeval *tv, void* tz) {
#define _W32_FT_OFFSET (116444736000000000ULL)
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	uint64_t value = ((uint64_t) ft.dwHighDateTime << 32) | ft.dwLowDateTime;
	tv->tv_usec = (long) ((value / 10ULL) % 1000000ULL);
	tv->tv_sec = (long) ((value - _W32_FT_OFFSET) / 10000000ULL);
	return (0);
}

bool HandlerRoutine(uint32_t dwCtrlType) {
	if (MAP_HAS1(_signalHandlers, dwCtrlType)) {
		_signalHandlers[dwCtrlType]();
		return true;
	}
	return false;
}

void installConfRereadSignal(SignalFnc pConfRereadSignalFnc) {
	_signalHandlers[CTRL_BREAK_EVENT] = pConfRereadSignalFnc;
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) HandlerRoutine, TRUE);
}

void installQuitSignal(SignalFnc pQuitSignalFnc) {
	_signalHandlers[CTRL_C_EVENT] = pQuitSignalFnc;
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) HandlerRoutine, TRUE);
}

double getFileModificationDate(const string &path) {
	struct _stat64 s;
	if (_stat64(path.c_str(), &s) != 0) {
		FATAL("Unable to stat file %s", STR(path));
		return 0;
	}
	return (double) s.st_mtime;
}

void InitNetworking() {
	WSADATA wsa;
	memset(&wsa, 0, sizeof (wsa));
	WSAStartup(0, &wsa);
	WSAStartup(wsa.wHighVersion, &wsa);
}

HMODULE UnicodeLoadLibrary(string fileName) {
	return LoadLibrary(STR(fileName));
}

int inet_aton(const char *pStr, struct in_addr *pRes) {
	pRes->S_un.S_addr = inet_addr(pStr);
	return true;
}

bool setMaxFdCount(uint32_t &current, uint32_t &max) {
	//1. Get the current values
	current = (uint32_t) _getmaxstdio();
	max = 2048;

	//2. Set the current value to the max value
	if (_setmaxstdio(2048) != 2048) {
		int err = errno;
		FATAL("_setmaxstdio failed: %d (%s)", err, strerror(err));
		return false;
	}

	//3. Get back the current value
	current = (uint32_t) _getmaxstdio();
	return true;
}

bool enableCoreDumps() {
	return true;
}

bool setFdCloseOnExec(SOCKET_TYPE fd) {
	return true;
}

bool setFdNonBlock(SOCKET_TYPE fd1) {
	u_long iMode = 1; // 0 for blocking, anything else for nonblocking

	if (ioctlsocket(fd1, FIONBIO, &iMode) < 0) {
		int err = SOCKET_LAST_ERROR;
		FATAL("Unable to set fd flags: %d", err);
		return false;
	}
	return true;
}

bool setFdNoSIGPIPE(SOCKET_TYPE fd) {
	return true;
}

void killProcess(pid_t pid) {
	//1. the the process snapshoot
	HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		FATAL("processes enumeration failed: %"PRIu32, err);
		return;
	}

	//2. Prepare the info structure
	PROCESSENTRY32 pe;
	memset(&pe, 0, sizeof (PROCESSENTRY32));
	pe.dwSize = sizeof (PROCESSENTRY32);

	//3. cycle over all processes and see which one has this one as parent
	if (Process32First(hSnap, &pe)) {
		do {
			if (pe.th32ParentProcessID == (DWORD) pid)
				killProcess(pe.th32ProcessID);
		} while (Process32Next(hSnap, &pe));
	}

	//4. Kill the process
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, (DWORD) pid);
	if (hProcess == NULL) {
		FATAL("Cannot access process id = %"PRId32, pid);
		return;
	}
	if (!TerminateProcess(hProcess, 0)) {
		FATAL("Cannot terminate process id = %"PRId32, pid);
	}
	CloseHandle(hProcess);
}

string normalizePath(const string &base, const string &file) {
#define MAX_WINDOWS_PATH 1024
	if (base == "")
		return normalizePath(".\\", file);
	if (base[base.size() - 1] != PATH_SEPARATOR)
		return normalizePath(base + PATH_SEPARATOR, file);

	char *pBase = new char[MAX_WINDOWS_PATH];
	char *pFile = new char[MAX_WINDOWS_PATH];
	if ((GetFullPathName(base.c_str(), MAX_WINDOWS_PATH, pBase, NULL) == 0)
			|| (GetFullPathName((base + file).c_str(), MAX_WINDOWS_PATH, pFile, NULL) == 0)) {
		delete[] pBase;
		delete[] pFile;
		return "";
	}

	string result = (memcmp(pBase, pFile, strlen(pBase)) == 0) ? (fileExists(pFile) ? pFile : "") : "";
	delete[] pBase;
	delete[] pFile;
	return result;
}

bool listFolder(const string &path, vector<string> &result, bool normalizeAllPaths,
		bool includeFolders, bool recursive) {
	WIN32_FIND_DATA ffd;
	TCHAR szDir[MAX_PATH];
	HANDLE hFind = INVALID_HANDLE_VALUE;
	DWORD dwError = 0;

	// Check that the input path plus 3 is not longer than MAX_PATH.
	// Three characters are for the "\*" plus NULL appended below.
	if (path.size() > (MAX_PATH - 3)) {
		WARN("Directory path is too long: %s.", STR(path));
		return false;
	}

	// Prepare string for use with FindFile functions.  First, copy the
	// string to a buffer, then append '\*' to the directory name.

	StringCchCopy(szDir, MAX_PATH, STR(path));
	StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

	// Find the first file in the directory.

	hFind = FindFirstFile(szDir, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		FATAL("Unable to open folder %s", STR(path));
		return false;
	}

	do {
		string entry = ffd.cFileName;
		if ((entry == ".") || (entry == "..")) {
			continue;
		}

		if (normalizeAllPaths) {
			entry = normalizePath(path, entry);
		} else {
			entry = path + entry;
		}
		if (entry == "")
			continue;

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (includeFolders) {
				ADD_VECTOR_END(result, entry);
			}
			if (recursive) {
				if (!listFolder(entry, result, normalizeAllPaths, includeFolders, recursive)) {
					FATAL("Unable to list folder");
					FindClose(hFind);
					return false;
				}
			}
		} else {
			ADD_VECTOR_END(result, entry);
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES) {
		WARN("Unable to find first file");
		FindClose(hFind);
		return false;
	}
	FindClose(hFind);
	return true;
}

bool deleteFolder(const string &path, bool force) {
	string fileFound;
	WIN32_FIND_DATA info;
	HANDLE hp;
	fileFound = format("%s\\*.*", STR(path));
	hp = FindFirstFile(STR(fileFound), &info);

	// Check first if we have a valid handle!
	if (hp == INVALID_HANDLE_VALUE) {
		WARN("Files to be deleted were already removed: %s", STR(fileFound));
		return true;
	}

	do {
		if (!((strcmp(info.cFileName, ".") == 0) ||
				(strcmp(info.cFileName, "..") == 0))) {
			if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
					FILE_ATTRIBUTE_DIRECTORY) {
				string subFolder = path;
				subFolder.append("\\");
				subFolder.append(info.cFileName);
				if (!deleteFolder(subFolder, true)) {
					FATAL("Unable to delete subfolder %s", STR(subFolder));
					return false;
				}
				if (!RemoveDirectory(STR(subFolder))) {
					FATAL("Unable to delete subfolder %s", STR(subFolder));
					return false;
				}
			} else {
				fileFound = format("%s\\%s", STR(path), info.cFileName);
				if (!DeleteFile(STR(fileFound))) {
					FATAL("Unable to delete file %s", STR(fileFound));
					return false;
				}
			}
		}
	} while (FindNextFile(hp, &info));
	FindClose(hp);
	return true;
}

bool createFolder(const string &path, bool recursive) {
	char DirName[256];
	const char* p = path.c_str();
	char* q = DirName;
	while (*p) {
		if (('\\' == *p) || ('/' == *p)) {
			if (':' != *(p - 1)) {
				CreateDirectory(DirName, NULL);
			}
		}
		*q++ = *p++;
		*q = '\0';
	}
	CreateDirectory(DirName, NULL);
	return true;
}

bool isAbsolutePath(const string &path) {
	if (path.size() < 4)
		return false;
	return (((path[0] >= 'A')&&(path[0] <= 'Z')) || ((path[0] >= 'a')&&(path[0] <= 'z')))
			&&(path[1] == ':')
			&&((path[2] == '\\') || (path[2] == '/')
			);
}

void GetFinishedProcesses(vector<pid_t> &pids, bool &noMorePids) {
	//1. set parameters to defaults
	pids.clear();
	noMorePids = true;

	// if pid cache is empty, bail out;
	if (_pids.size() == 0)
		return;
	//2. prepare handles of pids
	uint32_t pidCount = (uint32_t) _pids.size();
	HANDLE *hProcs = new HANDLE[pidCount];
	uint8_t i = 0;
	uint8_t truePidCount = 0;

	while (truePidCount != _pids.size()) {
		DWORD currProcId = _pids.back();

		hProcs[i] = OpenProcess(PROCESS_ALL_ACCESS, false, currProcId);
		if (hProcs[i] == 0) {
			ADD_VECTOR_END(pids, currProcId);
		} else {
			i++;
			truePidCount++;
			ADD_VECTOR_BEGIN(_pids, currProcId);
		}
		_pids.pop_back();
	}

	//3. catch any terminated process handles
	while (truePidCount) {
		pid_t procId = 0;
		DWORD resp = WaitForMultipleObjects(truePidCount, hProcs, FALSE, 0);
		if (resp == WAIT_TIMEOUT) // break when there is no terminated processes
			break;

		noMorePids = false;

		if (resp == WAIT_FAILED) {
			//ASSERT("Failed to get finished processes");
			WARN("Failed to get process status. Err: %"PRIu32, GetLastError());
			break;
		} else {

			//4. get process id from process handle and dump it into the pid param
			HANDLE hProcess = hProcs[resp];
			procId = GetProcessId(hProcess);
			CloseHandle(hProcess);
		}

		//5. refresh pid cache

		FOR_VECTOR_ITERATOR(pid_t, _pids, i) {
			if (VECTOR_VAL(i) == procId) {
				_pids.erase(i);
				break;
			}
		}
		pids.push_back(procId);

		//6. update handle array and close unused handle
		truePidCount--;
		memmove(&hProcs[resp], &hProcs[resp + 1], pidCount * sizeof (HANDLE));
	};
}

bool LaunchProcess(string fullBinaryPath, vector<string> &arguments, vector<string> &envVars, pid_t &pid) {
	// Wrap fullBinaryPath within qoutes so that spaces will be honored
	fullBinaryPath = "\"" + fullBinaryPath + "\"";

	// For arguments, qoute as needed as well

	FOR_VECTOR(arguments, i) {
		fullBinaryPath += " \"" + arguments[i] + "\"";
	}

	// Actual number of characters to be reserved for the environment variables
	size_t blockSize = 1;

	// Before creating the envVars, we need to copy the environment variables from the parent
	static vector<string> parentVars;
	static bool gotParentsVar = false;
	static uint32_t pvSize = 0;

	if (gotParentsVar == false) {
		// Populate it first
		parentVars.clear();
		const char* pVars = GetEnvironmentStrings();
		uint32_t prev = 0;
		pvSize = 0;

		do {
			if (pVars[pvSize] == '\0') {
				parentVars.push_back(string(pVars + prev, pVars + pvSize));

				// Skip the null terminator
				prev = pvSize + 1;
				if (pVars[pvSize + 1] == '\0') {
					pvSize += 1; // consider the last null terminator
					break;
				}
			}

			pvSize++;

		} while (pvSize < 0x0FFFFFFFF);

		// Toggle the field since we already got it from the parent
		//TODO: what if the client updated the environment and EMS was already called with launchProcess?
		gotParentsVar = true;
	}

	// Add the passed env variables to the size to be reserved

	FOR_VECTOR(envVars, i) {
		blockSize += envVars[i].length() + 1;
	}

	char *envBlock = NULL;

	// Sanity check: if limit reached
	if ((pvSize == 0x0FFFFFFFF) || ((uint32_t) (pvSize + blockSize) < pvSize)) {
		WARN("Passed environment variables will not be considered.");
	} else {
		// Add to the target blocksize
		blockSize += pvSize;

		// Create envVars block (null-terminated block of null-terminated key-value pair strings
		// format should be: name1=value1\0name2=value2\0\0
		if (envVars.size() != 0) {
			envBlock = new char[blockSize];
			memset(envBlock, 0, blockSize);
			char *p = envBlock;

			// First the parent

			FOR_VECTOR(parentVars, i) {
				memcpy(p, parentVars[i].c_str(), parentVars[i].length());
				p += parentVars[i].length() + 1;
			}

			// Then append the passed envVars

			FOR_VECTOR(envVars, i) {
				memcpy(p, envVars[i].c_str(), envVars[i].length());
				p += envVars[i].length() + 1;
			}
		}
	}

	//FINEST("blockSize: %"PRIu32, blockSize);

	// Create the process
	STARTUPINFO si = {0};
	si.cb = sizeof (si);
	PROCESS_INFORMATION pi = {0};
	char *pTemp = new char[fullBinaryPath.size() + 1];
	memset(pTemp, 0, fullBinaryPath.size() + 1);
	memcpy(pTemp, fullBinaryPath.data(), fullBinaryPath.size());
	//FINEST("pTemp: `%s`",pTemp);
	if (!CreateProcess(NULL, pTemp, NULL, NULL, false, 0, envBlock, NULL, &si, &pi)) {
		DWORD error = GetLastError();
		FATAL("Unable to launch proces. Error: %"PRIu32, error);
		delete[] pTemp;
		if (envBlock != NULL)
			delete[] envBlock;
		return false;
	}


	// Cleanup
	delete[] pTemp;
	if (envBlock != NULL)
		delete[] envBlock;
	pid = pi.dwProcessId;

	_pids.push_back(pid);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}

bool OpenSysLog(const string name) {
	return true;
}

void Syslog(int32_t level, const char *message, ...) {
}

void CloseSysLog() {
}

string realPath(const string &path) {
	char *pResult = new char[MAX_WINDOWS_PATH];
	string result;

	if (GetFullPathName(path.c_str(), MAX_WINDOWS_PATH, pResult, NULL) != 0)
		result = pResult;

	delete[] pResult;
	return result;
}

static unsigned int get_interface_flags(const IP_ADAPTER_ADDRESSES *pap) {
	unsigned int flags = IFF_UP;
	if (pap->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
		flags |= IFF_LOOPBACK;
	else if (pap->IfType == IF_TYPE_PPP
			|| pap->IfType == IF_TYPE_SLIP)
		flags |= IFF_POINTOPOINT;
	if (!(pap->Flags & IP_ADAPTER_NO_MULTICAST))
		flags |= IFF_MULTICAST;
	if (pap->OperStatus == IfOperStatusUp
			|| pap->OperStatus == IfOperStatusUnknown)
		flags |= IFF_RUNNING;
	return flags;



	/*if (pap->OperStatus != IfOperStatusLowerLayerDown)
		flags |= IFF_LOWER_UP;
	if (pap->OperStatus == IfOperStatusDormant)
		flags |= IFF_DORMANT;*/
}

struct sockaddr *get_net_mask(const sockaddr *pIp, int prefixLength) {
	struct sockaddr_in *pResult4 = NULL;
	struct sockaddr_in6 *pResult6 = NULL;
	if (pIp->sa_family == AF_INET) {
		pResult4 = new sockaddr_in;
		memset(pResult4, 0, sizeof (struct sockaddr_in));
		pResult4->sin_family = AF_INET;
		uint64_t address = (1ULL << prefixLength) - 1;
		pResult4->sin_addr.S_un.S_addr = (ULONG) address;
	} else if (pIp->sa_family == AF_INET6) {
		pResult6 = new sockaddr_in6;
		memset(pResult6, 0, sizeof (struct sockaddr_in6));
	} else {
		return NULL;
	}
	return (pIp->sa_family == AF_INET) ? (struct sockaddr *) pResult4 : (struct sockaddr *) pResult6;
}

int getifaddrs(struct ifaddrs **ppIfap) {
	//set the result to NULL before anything
	*ppIfap = NULL;

	//the flags
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	//GAA_FLAG_SKIP_ANYCAST |
	//GAA_FLAG_SKIP_UNICAST |
	//GAA_FLAG_SKIP_MULTICAST |
	//GAA_FLAG_SKIP_FRIENDLY_NAME;

	//get the required memory size
	ULONG needed = 0;
	if ((GetAdaptersAddresses(AF_INET, flags, NULL, NULL, &needed) != ERROR_BUFFER_OVERFLOW) || (needed == 0)) {
		errno = EINVAL;
		return -1;
	}

	//allocate the memory
	IP_ADAPTER_ADDRESSES *pTemp = (IP_ADAPTER_ADDRESSES *) HeapAlloc(GetProcessHeap(), 0, needed);

	//call it again
	if (GetAdaptersAddresses(AF_INET, flags, NULL, pTemp, &needed) != ERROR_SUCCESS) {
		HeapFree(GetProcessHeap(), 0, pTemp);
		errno = EINVAL;
		return -1;
	}

	//cycle over the returned stuff
	struct ifaddrs *pCursor = NULL;
	IP_ADAPTER_ADDRESSES *pAdapterCursor = pTemp;
	while (pAdapterCursor != NULL) {
		//create and 0 out struct ifaddrs
		if (pCursor == NULL) {
			pCursor = new struct ifaddrs;
			*ppIfap = pCursor;
		} else {
			pCursor->ifa_next = new struct ifaddrs;
			pCursor = pCursor->ifa_next;
		}

		//set its members
		pCursor->pRawItem = pAdapterCursor;
		pCursor->pRawHead = pTemp;
		pCursor->ifa_flags = get_interface_flags(pCursor->pRawItem);
		if (pCursor->pRawItem->FirstUnicastAddress != NULL) {
			pCursor->ifa_addr = pCursor->pRawItem->FirstUnicastAddress->Address.lpSockaddr;
			pCursor->ifa_netmask = get_net_mask(pCursor->ifa_addr, pCursor->pRawItem->FirstUnicastAddress->OnLinkPrefixLength);
		}
		pCursor->_data.ifi_mtu = pCursor->pRawItem->Mtu;
		if (pCursor->ifa_addr != NULL) {
			if (pCursor->ifa_addr->sa_family == AF_INET)
				pCursor->_data.ifi_metric = pCursor->pRawItem->Ipv4Metric;
			else if (pCursor->ifa_addr->sa_family == AF_INET6)
				pCursor->_data.ifi_metric = pCursor->pRawItem->Ipv6Metric;
		}

		pAdapterCursor = pAdapterCursor->Next;
	}
	//done
	return 0;
}

void freeifaddrs(struct ifaddrs *ifp) {
	if (ifp == NULL)
		return;
	HeapFree(GetProcessHeap(), 0, ifp->pRawHead);
	struct ifaddrs *pCursor = ifp;
	while (pCursor != NULL) {
		struct ifaddrs *pTemp = pCursor;
		pCursor = pCursor->ifa_next;
		delete pTemp;
	}
}

struct WSABUFWrapper {
	WSABUF *_pSendMsgBuffers;
	size_t _count;

	WSABUFWrapper() {
		_pSendMsgBuffers = NULL;
		_count = 0;
	}

	~WSABUFWrapper() {
		Clean();
	}

	void Clean() {
		if (_pSendMsgBuffers != NULL) {
			delete[] _pSendMsgBuffers;
			_pSendMsgBuffers = NULL;
		}
		_count = 0;
	}

	void Init(const struct iovec *pBuffers, size_t count) {
		if ((pBuffers == NULL) || (count == 0))
			return;
		if (count > _count) {
			Clean();
			_pSendMsgBuffers = new WSABUF[count];
			_count = count;
		}
		for (size_t i = 0; i < count; i++) {
			_pSendMsgBuffers[i].buf = (CHAR *) pBuffers[i].iov_base;
			_pSendMsgBuffers[i].len = (ULONG) pBuffers[i].iov_len;
		}
	}
};
static MUTEX_TYPE _gWsaBufferLock = MUTEX_STATIC_INIT;
static WSABUFWrapper _gWsaBuffer;

ssize_t sendmsg(SOCKET socket, const struct msghdr *msg, int flags) {
	LOCK(&_gWsaBufferLock);
	_gWsaBuffer.Init(msg->msg_iov, msg->msg_iovlen);
	DWORD bytesSent = 0;
	if (WSASendTo(socket, _gWsaBuffer._pSendMsgBuffers, (DWORD) msg->msg_iovlen, &bytesSent, flags, (const sockaddr *) msg->msg_name, (DWORD) msg->msg_namelen, NULL, NULL) != 0) {
		DWORD err = WSAGetLastError();
		FINEST("WSASendTo failed: %d", err);
		errno = EINVAL;
		return -1;
	}
	return (ssize_t) bytesSent;
}

void usleep(uint64_t microSeconds) {
	Sleep((DWORD) (microSeconds / 1000));
}

struct ThreadProcInfo {
	void *(*start_routine)(void *);
	void *pArguments;
};

DWORD WINAPI __ThreadProc__(LPVOID lpParameter) {
	ThreadProcInfo info = *((ThreadProcInfo *) lpParameter);
	delete (ThreadProcInfo *) lpParameter;
	info.start_routine(info.pArguments);
	return 0;
}

int THREAD_CREATE(THREAD_TYPE *pThread, void *pReserved, void *(*start_routine)(void *), void *pArguments) {
	if ((pThread == NULL) || (start_routine == NULL))
		return EINVAL;
	ThreadProcInfo *pInfo = new ThreadProcInfo;
	pInfo->start_routine = start_routine;
	pInfo->pArguments = pArguments;
	*pThread = CreateThread(NULL, 0, __ThreadProc__, pInfo, 0, NULL);
	if (*pThread == NULL) {
		delete pInfo;
		return GetLastError();
	}
	return 0;
}

int THREAD_JOIN(THREAD_TYPE thread, void **ppReserved) {
	DWORD result = WaitForSingleObject(thread, INFINITE);
	if (result != WAIT_OBJECT_0)
		return result;
	CloseHandle(thread);
	return 0;
}

void THREAD_EXIT(void *pReserved) {
	ExitThread(0);
}

int64_t getFileSize(int fd) {
	LARGE_INTEGER fileSize;
	if (GetFileSizeEx((HANDLE) _get_osfhandle(fd), &fileSize) == 0) {
		DWORD err = GetLastError();
		FATAL("Unable to obtain file size. Error was %"PRIu32, err);
		return -1;
	}
	return (int64_t) fileSize.QuadPart;
}

overlapped_wrapper_t::overlapped_wrapper_t() {
	_pOverlapped = NULL;
	_pEvents = NULL;
	_count = 0;
	_fd = -1;
	_fdHandle = INVALID_HANDLE_VALUE;
}

overlapped_wrapper_t::~overlapped_wrapper_t() {
	Free();
}

bool overlapped_wrapper_t::Init(size_t count) {
	//sanitize the parameter
	if (count == 0)
		return true;

	//if we have enough space, just return
	if (count <= _count)
		return true;

	//free existing overlapped structures and events
	Free();

	//set the new count
	_count = count;

	//allocate the overlapped structures
	_pOverlapped = new OVERLAPPED[_count];
	memset(_pOverlapped, 0, sizeof (OVERLAPPED) * _count);

	//allocate the events
	_pEvents = new HANDLE[_count];
	memset(_pEvents, 0, sizeof (HANDLE) * _count);

	//initialize the overlapped structures and events
	for (size_t i = 0; i < _count; i++) {
		_pOverlapped[i].Offset = 0xffffffff;
		_pOverlapped[i].OffsetHigh = 0xffffffff;
		_pOverlapped[i].hEvent = _pEvents[i] = CreateEvent(
				NULL, // default security attribute
				TRUE, // manual-reset event
				TRUE, // initial state = signaled
				NULL); // unnamed event object
		if (_pEvents[i] == INVALID_HANDLE_VALUE) {
			FATAL("Unable to create event");
			return false;
		}
	}

	//done
	return true;
}

void overlapped_wrapper_t::Free() {
	//free the overlapped structures
	if (_pOverlapped != NULL) {
		delete[] _pOverlapped;
	}
	_pOverlapped = NULL;

	//free the events
	if (_pEvents != NULL) {
		for (size_t i = 0; i < _count; i++) {
			CloseHandle(_pEvents[i]);
		}
		delete[] _pEvents;
	}
	_pEvents = NULL;

	//reset the count
	_count = 0;
}

ssize_t writev_ex(overlapped_wrapper_t &ow, const struct iovec *iov, int iovcnt) {
	//see if we have any vectors to write
	if (iovcnt <= 0)
		return 0;

	//initialize the vectors
	if (!ow.Init(iovcnt)) {
		FATAL("Initialization of overlapped windows structures failed");
		return -1;
	}

	//get the HANDLE from the file descriptor
	if (ow._fdHandle == INVALID_HANDLE_VALUE)
		ow._fdHandle = (HANDLE) _get_osfhandle(ow._fd);

	//do the overlapped writes
	ssize_t totalWritten = 0;
	for (int i = 0; i < iovcnt; i++) {
		//do the write
		if (!WriteFileEx(ow._fdHandle, iov[i].iov_base, (int) iov[i].iov_len, &ow._pOverlapped[i], NULL)) {
			DWORD err = GetLastError();
			FINEST("WriteFileEx failed: %d", (int) err);
			return -1;
		}
		totalWritten += iov[i].iov_len;
	}

	//wait for all writes to complete
	DWORD waitResult = WaitForMultipleObjects(iovcnt, ow._pEvents, true, INFINITE);
	if (waitResult != WAIT_OBJECT_0) {
		FINEST("WaitForMultipleObjects failed: %d", (int) waitResult);
		totalWritten = -1;
	}

	//done
	return totalWritten;
}

int fsync_ex(overlapped_wrapper_t &ow) {
	return 0;
	////get the HANDLE from the file descriptor
	//if (ow._fdHandle == INVALID_HANDLE_VALUE)
	//	ow._fdHandle = (HANDLE) _get_osfhandle(ow._fd);
	//return FlushFileBuffers(ow._fdHandle) != 0 ? 0 : -1;
}

#endif /* WIN32 */
