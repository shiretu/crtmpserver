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

class IOBuffer;

/**
 * This structure represents a message formed out of multiple continuous buffers.
 */
struct message_t {
	/**
	 * The collection of continuous buffers which are forming the message. The
	 * order in which they sit inside this pointer is the order in which they are
	 * inside the logical message
	 */
	struct iovec *_pBuffers;

	/**
	 * How many continuous buffers we have inside the message
	 */
	size_t _count;

	/**
	 * What is the total amount of bytes which is forming the message. It must be
	 * the sum of the lengths of all continuous buffers
	 */
	size_t _totalSize;

	/**
	 * Stores the current message into a IOBuffer instance
	 * @param pBuffer the destination buffer
	 * @return true if all message was successfully stored, false otherwise
	 */
	bool StoreToIOBuffer(IOBuffer *pBuffer) const;

	/**
	 * Pretty prints this instance
	 * @return the string representation
	 */
	std::string ToString() const;
};
