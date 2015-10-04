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

#include "utils/logging/baseloglocation.h"

class File;

class DLLEXP FileLogLocation
: public BaseLogLocation {
private:
	File *_pFile;
	uint32_t _fileMaxLength;
	uint32_t _fileCurrentLength;
	uint32_t _filePathIndex;
	string _fileName;
	uint32_t _fileMaxHistorySize;

	uint32_t _forkId;
	string _newLineCharacters;
public:
	FileLogLocation(Variant &configuration);
	virtual ~FileLogLocation();

	virtual bool Init();
	virtual void Log(int32_t level, const char *pFileName, uint32_t lineNumber,
			const char *pFunctionName, string &message);
	virtual void SignalFork(uint32_t forkId);
private:
	bool OpenFile();
	void CloseFile();
};
