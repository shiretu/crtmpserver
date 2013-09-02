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

string format(const char *pFormat, ...) {
	char *pBuffer = NULL;
	va_list arguments;
	va_start(arguments, pFormat);
	if (vasprintf(&pBuffer, pFormat, arguments) == -1) {
		va_end(arguments);
		o_assert(false);
		return "";
	}
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
	if (vasprintf(&pBuffer, pFormat, args) == -1) {
		o_assert(false);
		return "";
	}
	string result;
	if (pBuffer != NULL) {
		result = pBuffer;
		free(pBuffer);
	}
	return result;
}
