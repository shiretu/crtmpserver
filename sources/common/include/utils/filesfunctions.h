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

//files
DLLEXP bool fileExists(const string &path);
DLLEXP bool deleteFile(const string &path);
DLLEXP void splitFileName(const string &fileName, string &name, string &extension, char separator = '.');
DLLEXP double getFileModificationDate(const string &path);
DLLEXP bool moveFile(const string &src, const string &dst);
DLLEXP int64_t getFileSize(int fd);

//folders
DLLEXP bool createFolder(const string &path, bool recursive);
DLLEXP bool deleteFolder(const string &path, bool force);
DLLEXP bool listFolder(const string &path, vector<string> &result, bool normalizeAllPaths = true, bool includeFolders = false, bool recursive = true);

//misc
DLLEXP string normalizePath(const string &base, const string &file);
DLLEXP string realPath(const string &path);
DLLEXP bool isAbsolutePath(const string &path);
