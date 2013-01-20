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

#ifndef _READYFORSENDINTERFACE_H
#define	_READYFORSENDINTERFACE_H

class DLLEXP ReadyForSendInterface {
public:

	/*!
	 * @brief keep the compiler happy about destructor
	 */
	virtual ~ReadyForSendInterface() {

	}

	/*!
	 * @brief This function will be called by the framework when the I/O layer
	 * can do more I/O on the same handler which was previously used to do
	 * output
	 */
	virtual void ReadyForSend() = 0;
};


#endif	/* _READYFORSENDINTERFACE_H */
