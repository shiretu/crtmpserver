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

#include "mediaformats/readers/ts/tsdocument.h"
#include "mediaformats/readers/ts/tsparser.h"
#include "mediaformats/readers/ts/avcontext.h"

TSDocument::TSDocument(Metadata &metadata)
: BaseMediaDocument(metadata) {
	_chunkSizeDetectionCount = 0;
	_chunkSize = 0;
	_pParser = new TSParser(this);
	_chunkSizeErrors = false;
	_lastOffset = 0;
}

TSDocument::~TSDocument() {
	if (_pParser != NULL) {
		delete _pParser;
		_pParser = NULL;
	}
}

BaseInStream *TSDocument::GetInStream() {
	return NULL;
}

void TSDocument::SignalResetChunkSize() {
	_chunkSizeErrors = true;
}

void TSDocument::SignalPAT(TSPacketPAT *pPAT) {
}

void TSDocument::SignalPMT(TSPacketPMT *pPMT) {

}

void TSDocument::SignalPMTComplete() {

}

bool TSDocument::SignalStreamsPIDSChanged(map<uint16_t, TSStreamInfo> &streams) {
	return true;
}

bool TSDocument::SignalStreamPIDDetected(TSStreamInfo &streamInfo,
		BaseAVContext *pContext, PIDType type, bool &ignore) {
	if (pContext == NULL) {
		ignore = true;
		return true;
	}
	pContext->_pStreamCapabilities = &_streamCapabilities;
	ignore = false;
	return true;
}

void TSDocument::SignalUnknownPIDDetected(TSStreamInfo &streamInfo) {
}

bool TSDocument::FeedData(BaseAVContext *pContext, uint8_t *pData,
		uint32_t dataLength, double pts, double dts, bool isAudio) {
	AddFrame(pContext->_pts.time, pContext->_dts.time,
			isAudio ? MEDIAFRAME_TYPE_AUDIO : MEDIAFRAME_TYPE_VIDEO);
	if (isAudio) {
		_audioSamplesCount++;
	} else {
		_videoSamplesCount++;
	}
	return true;
}

bool TSDocument::ParseDocument() {
	if (!DetermineChunkSize()) {
		FATAL("Unable to determine chunk size");
		return false;
	}

	if (!_mediaFile.SeekTo(_chunkSizeDetectionCount)) {
		FATAL("Unable to seek at %"PRIu32, _chunkSizeDetectionCount);
		return false;
	}

	_pParser->SetChunkSize(_chunkSize);

	IOBuffer buffer;
	uint32_t defaultBlockSize = ((1024 * 1024 * 4) / _chunkSize) * _chunkSize;

	while (!_chunkSizeErrors) {
		uint32_t available = (uint32_t) (_mediaFile.Size() - _mediaFile.Cursor());
		if (available < _chunkSize) {
			break;
		}
		if (GETAVAILABLEBYTESCOUNT(buffer) != 0) {
			WARN("Leftovers detected");
			break;
		}
		uint32_t blockSize = defaultBlockSize < available ? defaultBlockSize : available;
		buffer.MoveData();
		if (!buffer.ReadFromFs(_mediaFile, blockSize)) {
			WARN("Unable to read %"PRIu32" bytes from file", blockSize);
			break;
		}
		if (!_pParser->ProcessBuffer(buffer, false)) {
			WARN("Unable to process block of data");
			break;
		}
	}
	return true;
}

bool TSDocument::BuildFrames() {
	return true;
}

Variant TSDocument::GetPublicMeta() {
	Variant result;
	if (_streamCapabilities.GetVideoCodec<VideoCodecInfo>() != NULL) {
		result["width"] = _streamCapabilities.GetVideoCodec<VideoCodecInfo>()->_width;
		result["height"] = _streamCapabilities.GetVideoCodec<VideoCodecInfo>()->_height;
	}
	return result;
}

void TSDocument::AddFrame(double pts, double dts, uint8_t frameType) {
	uint64_t currentOffset = _pParser->GetTotalParsedBytes();
	bool insert = false;
	if (_lastOffset == 0) {
		_lastOffset = currentOffset;
		insert = true;
	} else if ((currentOffset - _lastOffset) < 7 * 188) {
		insert = false;
	} else {
		insert = true;
	}
	if (!insert)
		return;
	MediaFrame frame;
	frame.cts = dts - pts;
	frame.dts = dts;
	frame.isBinaryHeader = false;
	frame.isKeyFrame = true;
	frame.length = 0;
	frame.pts = pts;
	frame.start = currentOffset + _chunkSizeDetectionCount;
	frame.type = frameType;
	ADD_VECTOR_END(_frames, frame);
	_lastOffset = currentOffset;
}

bool TSDocument::DetermineChunkSize() {
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

bool TSDocument::GetByteAt(uint64_t offset, uint8_t &byte) {
	uint64_t backup = _mediaFile.Cursor();
	if (!_mediaFile.SeekTo(offset)) {
		FATAL("Unable to seek to offset %"PRIu64, offset);
		return false;
	}

	if (!_mediaFile.ReadUI8(&byte)) {
		FATAL("Unable to read byte at offset %"PRIu64, offset);
		return false;
	}

	if (!_mediaFile.SeekTo(backup)) {
		FATAL("Unable to seek to offset %"PRIu64, backup);
		return false;
	}

	return true;
}

bool TSDocument::TestChunkSize(uint8_t chunkSize) {
	_chunkSize = 0;
	uint8_t byte;
	if ((int64_t) (_mediaFile.Size() - _mediaFile.Cursor()) <(2 * chunkSize + 1))
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

#endif /* HAS_MEDIA_TS */
