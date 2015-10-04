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

#include "common.h"

class BaseLogLocation;

class DLLEXP Version {
public:
	static string _lifeId;
	static uint16_t _instance;
	static string GetBuildNumber();
	static uint64_t GetBuildDate();
	static string GetBuildDateString();
	static string GetReleaseNumber();
	static string GetCodeName();
	static string GetBuilderOSName();
	static string GetBuilderOSVersion();
	static string GetBuilderOSArch();
	static string GetBuilderOSUname();
	static string GetBuilderOS();
	static string GetBanner();
	static Variant GetAll();
	static Variant GetBuilder();
};

class DLLEXP Logger {
private:
	static MUTEX_TYPE _lock;
	static Logger *_pLogger; //! Pointer to the Logger class.
	vector<BaseLogLocation *> _logLocations; //! Vector that stores the location of the log file.
	bool _freeAppenders; //! Boolean that releases the logger.
private:
	Logger();
public:
	virtual ~Logger();

	static void Init();
	static void Free(bool freeAppenders);
	static void Log(int32_t level, const char *pFileName, uint32_t lineNumber,
			const char *pFunctionName, const char *pFormatString, ...);
	static bool AddLogLocation(BaseLogLocation *pLogLocation);
	static void SignalFork(uint32_t forkId);
	static void SetLevel(int32_t level);
};
