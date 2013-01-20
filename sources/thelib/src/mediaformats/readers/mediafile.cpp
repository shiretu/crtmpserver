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

#include "mediaformats/readers/mediafile.h"

MediaFile* GetFile(string filePath, uint32_t windowSize) {
	MediaFile *pResult = new MediaFile();
#ifdef HAS_MMAP
	if (windowSize == 0)
		windowSize = 131072;
	if (!pResult->Initialize(filePath, windowSize)) {
		delete pResult;
		pResult = NULL;
	}
#else /* HAS_MMAP */
	if (!pResult->Initialize(filePath)) {
		delete pResult;
		pResult = NULL;
	}
#endif /* HAS_MMAP */
	return pResult;
}

void ReleaseFile(MediaFile *pFile) {
	if (pFile == NULL)
		return;
	delete pFile;
}

bool GetFile(string filePath, uint32_t windowSize, MediaFile &mediaFile) {
#ifdef HAS_MMAP
	if (windowSize == 0)
		windowSize = 131072;
	return mediaFile.Initialize(filePath, windowSize);
#else /* HAS_MMAP */
	return mediaFile.Initialize(filePath);
#endif /* HAS_MMAP */
}
