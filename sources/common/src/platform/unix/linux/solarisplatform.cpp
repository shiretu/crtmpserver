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

#ifdef SOLARIS

#include "platform/solaris/solarisplatform.h"
#include "common.h"

class SolarisPlatform {
public:

	SolarisPlatform() {
		//Ignore all SIGPIPE signals
		struct sigaction act;
		act.sa_handler = SIG_IGN;
		sigaction(SIGPIPE, &act, NULL);
	}

	virtual ~SolarisPlatform() {

	}
};
static SolarisPlatform _platform;

time_t timegm(struct tm *tm) {
	time_t ret;
	char *tz;

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	TzSet();
	ret = mktime(tm);
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	TzSet();
	return ret;
}

#endif /* SOLARIS */
