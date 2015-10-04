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

//#define SHOW_LOCKING_DEBUG

class Locker {
private:
	MUTEX_TYPE *_pMutex;
#ifdef SHOW_LOCKING_DEBUG
	const char *_pFile;
	const int _line;
#endif /* SHOW_LOCKING_DEBUG */
public:

	inline Locker(MUTEX_TYPE *pMutex
#ifdef SHOW_LOCKING_DEBUG
			, const char *pFile, int line)
	: _pFile(pFile), _line(line) {
#else /* SHOW_LOCKING_DEBUG */
			) {
#endif /* SHOW_LOCKING_DEBUG */
		_pMutex = pMutex;
#ifdef SHOW_LOCKING_DEBUG
		printf("Try locking: %s:%d\n", _pFile, _line);
#endif /* SHOW_LOCKING_DEBUG */
		if (MUTEX_LOCK(_pMutex) != 0) {
			fprintf(stderr, "Unable to lock the mutex");
			fflush(stderr);
			o_assert(false);
		}
#ifdef SHOW_LOCKING_DEBUG
		printf("     locked: %s:%d\n", _pFile, _line);
#endif /* SHOW_LOCKING_DEBUG */
	};

	inline virtual ~Locker() {
#ifdef SHOW_LOCKING_DEBUG
		printf("Try unlocking: %s:%d\n", _pFile, _line);
#endif /* SHOW_LOCKING_DEBUG */
		if (MUTEX_UNLOCK(_pMutex) != 0) {
			fprintf(stderr, "Unable to unlock the mutex");
			fflush(stderr);
			o_assert(false);
		}
#ifdef SHOW_LOCKING_DEBUG
		printf("     unlocked: %s:%d\n", _pFile, _line);
#endif /* SHOW_LOCKING_DEBUG */
	}
};


#ifdef SHOW_LOCKING_DEBUG
#define LOCK(mutex) Locker ___locker___(mutex,__FILE__,__LINE__)
#else /* SHOW_LOCKING_DEBUG */
#define LOCK(mutex) Locker ___locker___(mutex)
#endif /* SHOW_LOCKING_DEBUG */
