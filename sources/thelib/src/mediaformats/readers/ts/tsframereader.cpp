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

#ifdef HAS_MEDIA_TS

#include "mediaformats/readers/ts/tsframereader.h"
#include "mediaformats/readers/ts/avcontext.h"
#include "mediaformats/readers/ts/tsframereaderinterface.h"

TSFrameReader::TSFrameReader(TSFrameReaderInterface *pInterface)
: TSParser(this) {
	_pFile = NULL;
	_freeFile = false;
	_chunkSizeDetectionCount = 0;
	_chunkSize = 0;
	_chunkSizeErrors = false;
	_frameAvailable = false;
	_eof = true;
	_pInterface = pInterface;
	if (_pInterface == NULL) {
		ASSERT("TSFrame reader can't have NULL interface");
	}
}

TSFrameReader::~TSFrameReader() {
	FreeFile();
}

bool TSFrameReader::SetFile(string filePath) {
	FreeFile();
	_freeFile = true;
	_pFile = GetFile(filePath, 4 * 1024 * 1024);
	if (_pFile == NULL) {
		FATAL("Unable to open file %s", STR(filePath));
		return false;
	}
	if (!DetermineChunkSize()) {
		FATAL("Unable to determine chunk size");
		FreeFile();
		return false;
	}

	SetChunkSize(_chunkSize);
	if (!_pFile->SeekTo(_chunkSizeDetectionCount)) {
		FATAL("Unable to seek to the beginning of file");
		FreeFile();
		return false;
	}
	_eof = _pFile->IsEOF();

	_defaultBlockSize = ((1024 * 1024 * 2) / _chunkSize) * _chunkSize;

	return true;
}

bool TSFrameReader::IsEOF() {
	return _eof;
}

bool TSFrameReader::Seek(uint64_t offset) {
	if (_pFile == NULL)
		return false;
	return _pFile->SeekTo(offset);
}

bool TSFrameReader::ReadFrame() {
	_frameAvailable = false;
	if (GETAVAILABLEBYTESCOUNT(_chunkBuffer) < _chunkSize) {
		uint64_t available = _pFile->Size() - _pFile->Cursor();
		available = available < _defaultBlockSize ? available : _defaultBlockSize;
		available /= _chunkSize;
		available *= _chunkSize;
		if (available < _chunkSize) {
			_eof = true;
			return true;
		}
		_chunkBuffer.MoveData();
		if (!_chunkBuffer.ReadFromFs(*_pFile, (uint32_t) available)) {
			FATAL("Unable to read %"PRIu8" bytes from file", _chunkSize);
			return false;
		}
	}
	while ((!_chunkSizeErrors)
			&& (!_frameAvailable)
			&& (GETAVAILABLEBYTESCOUNT(_chunkBuffer) >= _chunkSize)) {
		if (!ProcessBuffer(_chunkBuffer, true)) {
			FATAL("Unable to process block of data");
			return false;
		}
	}
	return !_chunkSizeErrors;
}

BaseInStream *TSFrameReader::GetInStream() {
	if (_pInterface != NULL)
		return _pInterface->GetInStream();
	return NULL;
}

void TSFrameReader::SignalResetChunkSize() {
	_chunkSizeErrors = true;
}

void TSFrameReader::SignalPAT(TSPacketPAT *pPAT) {
	//NYI;
}

void TSFrameReader::SignalPMT(TSPacketPMT *pPMT) {
	//NYI;
}

void TSFrameReader::SignalPMTComplete() {
	//NYI;
}

bool TSFrameReader::SignalStreamsPIDSChanged(map<uint16_t, TSStreamInfo> &streams) {
	return true;
}

bool TSFrameReader::SignalStreamPIDDetected(TSStreamInfo &streamInfo,
		BaseAVContext *pContext, PIDType type, bool &ignore) {
	pContext->_pStreamCapabilities = &_streamCapabilities;
	ignore = false;
	return true;
}

void TSFrameReader::SignalUnknownPIDDetected(TSStreamInfo &streamInfo) {
	NYI;
}

bool TSFrameReader::FeedData(BaseAVContext *pContext, uint8_t *pData,
		uint32_t dataLength, double pts, double dts, bool isAudio) {
	if (!_pInterface->SignalFrame(pData, dataLength, pts, dts, isAudio)) {
		FATAL("Unable to feed frame");
		return false;
	}
	_frameAvailable = true;
	return true;
}

void TSFrameReader::FreeFile() {
	if ((_freeFile)&&(_pFile != NULL)) {
		ReleaseFile(_pFile);
	}
	_pFile = NULL;
}

bool TSFrameReader::DetermineChunkSize() {
	while (true) {
		if (_chunkSizeDetectionCount >= TS_CHUNK_208) {
			FATAL("I give up. I'm unable to detect the ts chunk size");
			return false;
		}

		//FINEST("Check for %"PRIu8" at %"PRIu8, (uint8_t) TS_CHUNK_188, _chunkSizeDetectionCount);
		if (!TestChunkSize(TS_CHUNK_188)) {
			FATAL("I give up. I'm unable to detect the ts chunk size");
			return false;
		}
		if (_chunkSize != 0)
			return true;

		//FINEST("Check for %"PRIu8" at %"PRIu8, (uint8_t) TS_CHUNK_204, _chunkSizeDetectionCount);
		if (!TestChunkSize(TS_CHUNK_204)) {
			FATAL("I give up. I'm unable to detect the ts chunk size");
			return false;
		}
		if (_chunkSize != 0)
			return true;

		//FINEST("Check for %"PRIu8" at %"PRIu8, (uint8_t) TS_CHUNK_208, _chunkSizeDetectionCount);
		if (!TestChunkSize(TS_CHUNK_208)) {
			FATAL("I give up. I'm unable to detect the ts chunk size");
			return false;
		}
		if (_chunkSize != 0)
			return true;

		_chunkSizeDetectionCount++;
	}
}

bool TSFrameReader::TestChunkSize(uint8_t chunkSize) {
	_chunkSize = 0;
	uint8_t byte;
	if ((int64_t)(_pFile->Size() - _pFile->Cursor()) <(2 * chunkSize + 1))
		return true;

	if (!GetByteAt(_chunkSizeDetectionCount, byte)) {
		FATAL("Unable to read byte at offset %"PRIu32, _chunkSizeDetectionCount);
		return false;
	}

	if (byte != 0x47)
		return true;

	if (!GetByteAt(_chunkSizeDetectionCount + chunkSize, byte)) {
		FATAL("Unable to read byte at offset %"PRIu32, _chunkSizeDetectionCount + chunkSize);
		return false;
	}

	if (byte != 0x47)
		return true;

	if (!GetByteAt(_chunkSizeDetectionCount + 2 * chunkSize, byte)) {
		FATAL("Unable to read byte at offset %"PRIu32, _chunkSizeDetectionCount + 2 * chunkSize);
		return false;
	}

	if (byte != 0x47)
		return true;

	_chunkSize = chunkSize;
	return true;
}

bool TSFrameReader::GetByteAt(uint64_t offset, uint8_t &byte) {
	uint64_t backup = _pFile->Cursor();
	if (!_pFile->SeekTo(offset)) {
		FATAL("Unable to seek to offset %"PRIu64, offset);
		return false;
	}

	if (!_pFile->ReadUI8(&byte)) {
		FATAL("Unable to read byte at offset %"PRIu64, offset);
		return false;
	}

	if (!_pFile->SeekTo(backup)) {
		FATAL("Unable to seek to offset %"PRIu64, backup);
		return false;
	}

	return true;
}
#endif	/* HAS_MEDIA_TS */
