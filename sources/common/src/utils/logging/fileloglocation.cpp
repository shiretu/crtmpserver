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

FileLogLocation::FileLogLocation(Variant &configuration)
: BaseLogLocation(configuration) {
	_pFile = NULL;
	_fileMaxLength = 0;
	_fileCurrentLength = 0;
	_filePathIndex = 1;
	_fileName = "";
	_fileMaxHistorySize = 0;

	_forkId = 0;
	_newLineCharacters = "\n";
}

FileLogLocation::~FileLogLocation() {
	CloseFile();
}

bool FileLogLocation::Init() {
	//base class init
	if (!BaseLogLocation::Init())
		return false;

	//gather the required info
	if (!_configuration.HasKeyChain(V_STRING, false, 1,
			CONF_LOG_APPENDER_FILE_NAME))
		return false;
	_fileName = (string) _configuration.GetValue(
			CONF_LOG_APPENDER_FILE_NAME, false);
	if (_configuration.HasKeyChain(V_STRING, false, 1,
			CONF_LOG_APPENDER_NEW_LINE_CHARACTERS))
		_newLineCharacters = (string) _configuration.GetValue(
			CONF_LOG_APPENDER_NEW_LINE_CHARACTERS, false);
	if (_configuration.HasKeyChain(_V_NUMERIC, false, 1,
			CONF_LOG_APPENDER_FILE_HISTORY_SIZE))
		_fileMaxHistorySize = (uint32_t) _configuration.GetValue(
			CONF_LOG_APPENDER_FILE_HISTORY_SIZE, false);
	if (_configuration.HasKeyChain(_V_NUMERIC, false, 1,
			CONF_LOG_APPENDER_FILE_LENGTH))
		_fileMaxLength = (uint32_t) _configuration.GetValue(
			CONF_LOG_APPENDER_FILE_LENGTH, false);

	//open the file
	return OpenFile();
}

void FileLogLocation::Log(int32_t level, const char *pFileName,
		uint32_t lineNumber, const char *pFunctionName, string &message) {
	if (_pFile == NULL) {
		if (!OpenFile())
			return;
	}
	string logEntry = format("%"PRIu64":%d:%s:%u:%s:%s",
			(uint64_t) time(NULL), level, pFileName, lineNumber, pFunctionName,
			STR(message));
	if (_singleLine) {
		replace(logEntry, "\r", "\\r");
		replace(logEntry, "\n", "\\n");
	}
	logEntry += _newLineCharacters;
	_pFile->WriteString(logEntry);
	_pFile->Flush();
	if ((_fileMaxLength > 0)&&(_fileMaxHistorySize > 0)) {
		_fileCurrentLength += (uint32_t) logEntry.length();
		if (_fileCurrentLength >= _fileMaxLength)
			CloseFile();
	}
}

void FileLogLocation::SignalFork(uint32_t forkId) {
	_forkId = forkId;
	CloseFile();
}

bool FileLogLocation::OpenFile() {
	//close the current file, just to make sure
	CloseFile();

	//default the file name to the specified value
	string fileName = format("%s.%02"PRIu32".log",
					_fileName.c_str(),
					_forkId
					);

	//roll the files if needed
	if ((_fileMaxLength > 0)&&(_fileMaxHistorySize > 0)) {
		for (ssize_t i = (_fileMaxHistorySize - 1); i >= 0; i--) {
			string src = "";
			if (i == 0)
				src = fileName;
			else
				src = format("%s.%02"PRIu32".%08"PRIz"u.log",
					_fileName.c_str(),
					_forkId,
					i
					);
			string dst = format("%s.%02"PRIu32".%08"PRIz"u.log",
					_fileName.c_str(),
					_forkId,
					i + 1
					);
			rename(src.c_str(), dst.c_str());
		}
	}

	//create and initialize the file
	_pFile = new File();
	if (!_pFile->Initialize(fileName, FILE_OPEN_MODE_TRUNCATE)) {
		CloseFile();
		return false;
	}

	//compute and write the header
	string header = format("PID: %"PRIu64"; TIMESTAMP: %"PRIz"u; LIFE: %s%s%s%s",
			(uint64_t) GetPid(),
			time(NULL),
			STR(Version::_lifeId),
			STR(_newLineCharacters),
			STR(Version::GetBanner()),
			STR(_newLineCharacters));
	if (!_pFile->WriteString(header)) {
		CloseFile();
		return false;
	}

	//done
	return true;
}

void FileLogLocation::CloseFile() {
	if (_pFile != NULL) {
		delete _pFile;
		_pFile = NULL;
	}
	_fileCurrentLength = 0;
}
