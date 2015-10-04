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

std::string message_t::ToString() const {
	IOBuffer b;
	for (size_t i = 0; i < _count; i++)
		b.ReadFromBuffer((const uint8_t *) _pBuffers[i].iov_base, (uint32_t) _pBuffers[i].iov_len);
	return format("iovec count: %"PRIz"u\niovec total size: %"PRIz"u\n", _count, _totalSize) + b.ToString();
}

bool message_t::StoreToIOBuffer(IOBuffer *pBuffer) const{
	if (pBuffer == NULL) {
		FATAL("Invalid destination buffer provided");
		return false;
	}

	for (size_t i = 0; i < _count; i++)
		if (!pBuffer->ReadFromBuffer((const uint8_t *) _pBuffers[i].iov_base, (uint32_t) _pBuffers[i].iov_len))
			return false;

	return true;
}
