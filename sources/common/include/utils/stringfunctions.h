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

DLLEXP bool EMSStringEqual(const std::string &str1, const std::string &str2,
		bool caseSensitive);
DLLEXP bool EMSStringEqual(const std::string &str1, const char *pStr2,
		bool caseSensitive);
DLLEXP bool EMSStringEqual(const std::string &str1, const char *pStr2,
		size_t str2length, bool caseSensitive);
DLLEXP bool EMSStringEqual(const char *EMS_RESTRICT pStr1,
		const char *EMS_RESTRICT pStr2, const size_t length, bool caseSensitive);
DLLEXP string & replace(string &target, const string &search, const string &replacement);
DLLEXP string &lowerCase(string &value);
DLLEXP string &upperCase(string &value);
DLLEXP bool isInteger(const string &str, int64_t &value);
DLLEXP bool isInteger(const char *pBuffer, size_t length, int64_t &value);
DLLEXP string format(const char *pFormat, ...);
DLLEXP string vFormat(const char *pFormat, va_list args);
DLLEXP string tagToString(uint64_t tag);
DLLEXP void lTrim(string &value);
DLLEXP void rTrim(string &value);
DLLEXP void trim(string &value);
DLLEXP void split(const string &str, const string &separator, vector<string> &result);
DLLEXP map<string, string> &mapping(map<string, string> &result,
		const string &str, const string &separator1, const string &separator2,
		bool trimStrings);
DLLEXP string generateRandomString(uint32_t length);
