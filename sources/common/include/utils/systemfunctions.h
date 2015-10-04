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

#pragma once

DLLEXP string GetEnvVariable(const char *pEnvVarName);
DLLEXP bool setMaxFdCount(uint32_t &current, uint32_t &max);
DLLEXP bool enableCoreDumps();
DLLEXP void killProcess(pid_t pid);
DLLEXP int8_t getCPUCount();
DLLEXP time_t getlocaltime();
DLLEXP time_t gettimeoffset();
DLLEXP void GetFinishedProcesses(vector<pid_t> &pids, bool &noMorePids);
DLLEXP bool LaunchProcess(string fullBinaryPath, vector<string> &arguments, vector<string> &envVars, pid_t &pid);
DLLEXP bool OpenSysLog(const string name);
DLLEXP void Syslog(int32_t level, const char *message, ...);
DLLEXP void CloseSysLog();
DLLEXP void installQuitSignal(SignalFnc pQuitSignalFnc);
DLLEXP void installConfRereadSignal(SignalFnc pConfRereadSignalFnc);
DLLEXP string getHostByName(const string &name);
DLLEXP uint64_t GetTimeMillis();
