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

bool EMSStringEqual(const std::string &str1, const std::string &str2,
		bool caseSensitive) {
	std::string::size_type length = 0;
	return ((length = str1.length()) != str2.length()) ?
			false
			:
			(caseSensitive ?
			(memcmp(str1.c_str(), str2.c_str(), length) == 0)
			:
			(strncasecmp(str1.c_str(), str2.c_str(), length) == 0));
}

bool EMSStringEqual(const std::string &str1, const char *pStr2,
		bool caseSensitive) {
	return (pStr2 == NULL) ?
			false
			:
			(caseSensitive ?
			(strcmp(str1.c_str(), pStr2) == 0)
			:
			(strcasecmp(str1.c_str(), pStr2) == 0));
}

DLLEXP bool EMSStringEqual(const std::string &str1, const char *pStr2,
		size_t str2length, bool caseSensitive) {
	return (str1.length() != str2length) ?
			false
			:
			(caseSensitive ?
			(memcmp(str1.c_str(), pStr2, str2length) == 0)
			:
			(strncasecmp(str1.c_str(), pStr2, str2length) == 0));
}

bool EMSStringEqual(const char *EMS_RESTRICT pStr1,
		const char *EMS_RESTRICT pStr2, const size_t length, bool caseSensitive) {
	return ((pStr1 == pStr2) || (length == 0)) ?
			true
			:
			(((pStr1 == NULL) || (pStr2 == NULL)) ?
			false
			:
			(caseSensitive ?
			(strncmp(pStr1, pStr2, length) == 0)
			:
			(strncasecmp(pStr1, pStr2, length) == 0)));
}

string & replace(string &target, const string &search, const string &replacement) {
	if ((search.size() == 0)
			|| ((search.size() == replacement.size()) && (search == replacement))
			)
		return target;
	string::size_type i = string::npos;
	string::size_type lastPos = 0;
	while ((i = target.find(search, lastPos)) != string::npos) {
		target.replace(i, search.length(), replacement);
		lastPos = i + replacement.length();
	}
	return target;
}

string &lowerCase(string &value) {
	std::transform(value.begin(), value.end(), value.begin(), ::tolower);
	return value;
}

string &upperCase(string &value) {
	std::transform(value.begin(), value.end(), value.begin(), ::toupper);
	return value;
}

DLLEXP bool isInteger(const string &str, int64_t &value) {
	return isInteger(str.c_str(), str.length(), value);
}

bool isInteger(const char *pBuffer, size_t length, int64_t &value) {
	value = 0;
	//consistency on the input parameters
	if ((pBuffer == NULL) //empty buffer
			|| (length == 0) //empty buffer
			)
		return false;

	//initialize the cursor
	const char *pCursor = pBuffer;

	//eat all leading spaces
	while (((size_t) (pCursor - pBuffer) < length) && isspace(pCursor[0]))
		pCursor++;

	//do we have anything left?
	if ((size_t) (pCursor - pBuffer) == length)
		return false;

	//read the sign and also position the cursor for further parsing
	bool negative = pCursor[0] == '-';
	pCursor += (negative || (pCursor[0] == '+')) ? 1 : 0;

	//do we have anything left?
	if ((size_t) (pCursor - pBuffer) == length)
		return false;

	//determine the nature of the number: octal, decimal or hexadecimal
	uint8_t base = 0;
	if (pCursor[0] == '0') {
		//if the string starts with 0, than is either an octal or a hexadecimal
		pCursor++;

		//if this was the last character, return true: it is a number and is 0
		//do we have anything left?
		if ((size_t) (pCursor - pBuffer) == length)
			return true;

		if ((pCursor[0] == 'x') || (pCursor[0] == 'X')) {
			//if the next character is 'x' or 'X', than this is a hexadecimal string
			pCursor++;

			//if this was the last character, than this is invalid: string
			//contains a 0x or 0X without anything following it
			if ((size_t) (pCursor - pBuffer) == length)
				return false;

			//this SHOULD be hexadecimal
			base = 16;
		} else {
			//this SHOULD be octal
			base = 8;
		}
	} else if ((pCursor[0] == 'b') || (pCursor[0] == 'B')) {
		pCursor++;

		//if this was the last character, return false. We need more
		if ((size_t) (pCursor - pBuffer) == length)
			return false;

		//this SHOULD be binary
		base = 2;
	} else {
		//this is a decimal
		base = 10;
	}

	//compute the cut-off, and cut-limit
	int64_t cutoff = (int64_t) (negative ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL);
	int cutlim = (int) (cutoff % base);
	cutoff /= base;
	if (negative)
		cutlim = -cutlim;

	int c;
	//printf("\n---------- %d ---------\n", base);
	for (; (size_t) (pCursor - pBuffer) < length; pCursor++) {
		//printf("%"PRIx64" -> ", value);
		if (isdigit(pCursor[0]))
			c = pCursor[0] - '0';
		else if (isalpha(pCursor[0]))
			c = pCursor[0]-(isupper(pCursor[0]) ? 'A' : 'a') + 10;
		else
			return false;
		if (c >= base)
			return false;
		switch (base) {
			case 2:
			{
				if ((value >> 63) != 0)
					return false;
				value = (value << 1) | c;
				break;
			}
			case 8:
			{
				if ((value >> 61) != 0)
					return false;
				value = (value << 3) | c;
				break;
			}
			case 10:
			{
				if (negative) {
					if ((value < cutoff) || ((value == cutoff)&&(c > cutlim)))
						return false;
					else
						value = value * base - c;
				} else {
					if ((value > cutoff) || ((value == cutoff)&&(c > cutlim)))
						return false;
					else
						value = value * base + c;
				}
				break;
			}
			case 16:
			{
				if ((value >> 60) != 0)
					return false;
				value = (value << 4) | c;
				break;
			}
		}
		//printf("%"PRIx64"\n", value);
	}

	return true;
}

string format(const char *pFormat, ...) {
	char *pBuffer = NULL;
	va_list arguments;
	va_start(arguments, pFormat);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif /* __clang__ */
	if (vasprintf(&pBuffer, pFormat, arguments) == -1) {
		va_end(arguments);
		o_assert(false);
		return "";
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif /* __clang__ */
	va_end(arguments);
	string result;
	if (pBuffer != NULL) {
		result = pBuffer;
		free(pBuffer);
	}
	return result;
}

string vFormat(const char *pFormat, va_list args) {
	char *pBuffer = NULL;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif /* __clang__ */
	if (vasprintf(&pBuffer, pFormat, args) == -1) {
		o_assert(false);
		return "";
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif /* __clang__ */
	string result;
	if (pBuffer != NULL) {
		result = pBuffer;
		free(pBuffer);
	}
	return result;
}

string tagToString(uint64_t tag) {
	string result;
	for (uint32_t i = 0; i < 8; i++) {
		uint8_t v = (tag >> ((7 - i)*8)&0xff);
		if (v == 0)
			break;
		result += (char) v;
	}
	return result;
}

void lTrim(string &value) {
	string::size_type i = 0;
	for (i = 0; i < value.length(); i++) {
		if (value[i] != ' ' &&
				value[i] != '\t' &&
				value[i] != '\n' &&
				value[i] != '\r')
			break;
	}
	value = value.substr(i);
}

void rTrim(string &value) {
	int32_t i = 0;
	for (i = (int32_t) value.length() - 1; i >= 0; i--) {
		if (value[i] != ' ' &&
				value[i] != '\t' &&
				value[i] != '\n' &&
				value[i] != '\r')
			break;
	}
	value = value.substr(0, i + 1);
}

void trim(string &value) {
	lTrim(value);
	rTrim(value);
}

void split(const string &str, const string &separator, vector<string> &result) {
	result.clear();
	string::size_type position = str.find(separator);
	string::size_type lastPosition = 0;
	size_t separatorLength = separator.length();

	while (position != str.npos) {
		ADD_VECTOR_END(result, str.substr(lastPosition, position - lastPosition));
		lastPosition = position + separatorLength;
		position = str.find(separator, lastPosition);
	}
	ADD_VECTOR_END(result, str.substr(lastPosition, string::npos));
}

map<string, string> &mapping(map<string, string> &result,
		const string &str, const string &separator1, const string &separator2,
		bool trimStrings) {
	vector<string> pairs;
	split(str, separator1, pairs);

	FOR_VECTOR_ITERATOR(string, pairs, i) {
		if (VECTOR_VAL(i) != "") {
			if (VECTOR_VAL(i).find(separator2) != string::npos) {
				string key = VECTOR_VAL(i).substr(0, VECTOR_VAL(i).find(separator2));
				string value = VECTOR_VAL(i).substr(VECTOR_VAL(i).find(separator2) + 1);
				if (trimStrings) {
					trim(key);
					trim(value);
				}
				result[key] = value;
			} else {
				if (trimStrings) {
					trim(VECTOR_VAL(i));
				}
				result[VECTOR_VAL(i)] = "";
			}
		}
	}
	return result;
}

static const char *pkAlowedCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static const size_t kAlowedCharactersSize = 62;
static uint8_t randBytes[16];

string generateRandomString(uint32_t length) {
	RAND_bytes(randBytes, sizeof (randBytes));
	string result = "";
	for (uint32_t i = 0; i < length; i++)
		result += pkAlowedCharacters[randBytes[i % sizeof (randBytes)] % kAlowedCharactersSize];
	return result;
}
