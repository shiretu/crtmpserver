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
	_fileStream = NULL;
	_canLog = false;
	_counter = 0;
	_newLineCharacters = "\n";
	_fileHistorySize = 0;
	_fileLength = 0;
	_currentLength = 0;
	_fileIsClosed = true;
}

FileLogLocation::~FileLogLocation() {
	CloseFile();
}

bool FileLogLocation::Init() {
	if (!BaseLogLocation::Init())
		return false;
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
		_fileHistorySize = (uint32_t) _configuration.GetValue(
			CONF_LOG_APPENDER_FILE_HISTORY_SIZE, false);
	if (_configuration.HasKeyChain(_V_NUMERIC, false, 1,
			CONF_LOG_APPENDER_FILE_LENGTH))
		_fileLength = (uint32_t) _configuration.GetValue(
			CONF_LOG_APPENDER_FILE_LENGTH, false);
	if (!OpenFile())
		return false;
	return true;
}

bool FileLogLocation::EvalLogLevel(int32_t level, const char *pFileName,
		uint32_t lineNumber, const char *pFunctionName) {
	if (!_canLog)
		return false;
	return BaseLogLocation::EvalLogLevel(level, pFileName, lineNumber, pFunctionName);
}

void FileLogLocation::Log(int32_t level, const char *pFileName,
		uint32_t lineNumber, const char *pFunctionName, string &message) {
	if (_fileIsClosed) {
		OpenFile();
		if (_fileIsClosed)
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
	_fileStream->WriteString(logEntry);
	_fileStream->Flush();
	if (_fileLength > 0) {
		_currentLength += (uint32_t) logEntry.length();
		if (_fileLength < _currentLength)
			OpenFile();
	}
}

void FileLogLocation::SignalFork() {
	_fileIsClosed = true;
	_history.clear();
}

bool FileLogLocation::OpenFile() {
	CloseFile();
	double ts;
	GETCLOCKS(ts, double);
	ts = (ts / CLOCKS_PER_SECOND) * 1000;
	string filename = format("%s.%"PRIu64".%"PRIu64".log", STR(_fileName), (uint64_t) GetPid(), (uint64_t) ts);
	_fileStream = new File();
	if (!_fileStream->Initialize(filename, FILE_OPEN_MODE_TRUNCATE)) {
		return false;
	}
	string header = format("PID: %"PRIu64"; TIMESTAMP: %"PRIz"u%s%s%s",
			(uint64_t) GetPid(),
			time(NULL),
			STR(_newLineCharacters),
			STR(Version::GetBanner()),
			STR(_newLineCharacters));
	if (!_fileStream->WriteString(header)) {
		return false;
	}
	if (_fileHistorySize > 0) {
		ADD_VECTOR_END(_history, filename);
		while (_history.size() > _fileHistorySize) {
			deleteFile(_history[0]);
			_history.erase(_history.begin());
		}
	}
	_currentLength = 0;
	_canLog = true;
	_fileIsClosed = false;
	return true;
}

void FileLogLocation::CloseFile() {
	if (_fileStream != NULL) {
		delete _fileStream;
		_fileStream = NULL;
	}
	_fileIsClosed = true;
	_canLog = false;
}
