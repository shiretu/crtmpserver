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


#include "streaming/baseinfilestream.h"
#include "streaming/basestream.h"
#include "streaming/baseoutstream.h"
#include "streaming/streamstypes.h"
#include "protocols/baseprotocol.h"
#include "mediaformats/readers/basemediadocument.h"
#include "mediaformats/readers/flv/flvdocument.h"
#include "mediaformats/readers/mp3/mp3document.h"
#include "mediaformats/readers/mp4/mp4document.h"
#include "application/baseclientapplication.h"
#include "mediaformats/readers/ts/tsdocument.h"
#include "mediaformats/readers/ts/tsframereader.h"

#define MMAP_MIN_WINDOW_SIZE (64*1024)
#define MMAP_MAX_WINDOW_SIZE (1024*1024)

BaseInFileStream::InFileStreamTimer::InFileStreamTimer(BaseInFileStream *pInFileStream) {
	_pInFileStream = pInFileStream;
}

BaseInFileStream::InFileStreamTimer::~InFileStreamTimer() {

}

void BaseInFileStream::InFileStreamTimer::ResetStream() {
	_pInFileStream = NULL;
}

bool BaseInFileStream::InFileStreamTimer::TimePeriodElapsed() {
	if (_pInFileStream != NULL)
		_pInFileStream->ReadyForSend();
	return true;
}

#define FILE_STREAMING_STATE_PAUSED 0
#define FILE_STREAMING_STATE_PLAYING 1
#define FILE_STREAMING_STATE_FINISHED 2

BaseInFileStream::BaseInFileStream(BaseProtocol *pProtocol, uint64_t type,
		string name)
: BaseInStream(pProtocol, type, name) {
	if (!TAG_KIND_OF(type, ST_IN_FILE)) {
		ASSERT("Incorrect stream type. Wanted a stream type in class %s and got %s",
				STR(tagToString(ST_IN_FILE)), STR(tagToString(type)));
	}
	_pTimer = NULL;
	_pSeekFile = NULL;
	_pFile = NULL;

	//frame info
	_totalFrames = 0;
	_currentFrameIndex = 0;
	memset(&_currentFrame, 0, sizeof (MediaFrame));

	//timing info
	_totalSentTime = 0;
	_totalSentTimeBase = 0;
	_startFeedingTime = 0;

	//buffering info
	_clientSideBufferLength = 0;

	//current state info
	_streamingState = FILE_STREAMING_STATE_PAUSED;
	_audioVideoCodecsSent = false;

	_seekBaseOffset = 0;
	_framesBaseOffset = 0;
	_timeToIndexOffset = 0;

	_streamCapabilities.Clear();

	_playLimit = -1;

	_highGranularityTimers = false;

	_keepClientBufferFull = false;
	_tsChunkSize = 0;
	_tsChunkStart = 0;
	_tsPts = 0;
	_tsDts = 0;
	_servedBytesCount = 0;
}

BaseInFileStream::~BaseInFileStream() {
	if ((GetProtocol() != NULL)&&(GetProtocol()->GetLastKnownApplication() != NULL))
		GetProtocol()->GetLastKnownApplication()->GetStreamMetadataResolver()->UpdateStats(
			_metadata.mediaFullPath(),
			_metadata.statsFileFullPath(),
			STATS_OPERATION_INCREMENT_SERVED_BYTES_COUNT, _servedBytesCount);
	if (_pTimer != NULL) {
		_pTimer->ResetStream();
		_pTimer->EnqueueForDelete();
		_pTimer = NULL;
	}
	ReleaseFile(_pSeekFile);
	ReleaseFile(_pFile);
}

void BaseInFileStream::SetClientSideBuffer(uint32_t value) {
	if (value == 0) {
		//WARN("Invalid client side buffer value: %"PRIu32, value);
		return;
	}
	if (value > 120) {
		value = 120;
	}
	if (_clientSideBufferLength > value) {
		//WARN("Client side buffer must be bigger than %"PRIu32, _clientSideBufferLength);
		return;
	}
	//	FINEST("Client side buffer modified: %"PRIu32" -> %"PRIu32,
	//			_clientSideBufferLength, value);
	_clientSideBufferLength = value;
}

uint32_t BaseInFileStream::GetClientSideBuffer() {
	return _clientSideBufferLength;
}

void BaseInFileStream::KeepClientBufferFull(bool value) {
	_keepClientBufferFull = value;
}

bool BaseInFileStream::KeepClientBufferFull() {
	return _keepClientBufferFull;
}

bool BaseInFileStream::StreamCompleted() {
	if (_currentFrameIndex >= _totalFrames)
		return true;
	if ((_playLimit >= 0) && (_playLimit < _totalSentTime))
		return true;
	return false;
}

StreamCapabilities * BaseInFileStream::GetCapabilities() {
	return &_streamCapabilities;
}

bool BaseInFileStream::Initialize(Metadata &metadata, TimerType timerType,
		uint32_t granularity) {
	_metadata = metadata;
	//1. Check to see if we have an universal seeking file
	string seekFilePath = _metadata.seekFileFullPath();
	if (!fileExists(seekFilePath)) {
		FATAL("Seek file not found");
		return false;
	}

	//2. Open the seek file
	_pSeekFile = GetFile(seekFilePath, 128 * 1024);
	if (_pSeekFile == NULL) {
		FATAL("Unable to open seeking file %s", STR(seekFilePath));
		return false;
	}

	//3. read stream capabilities
	uint32_t streamCapabilitiesSize = 0;
	IOBuffer raw;
	//	if(!_pSeekFile->SeekBegin()){
	//		FATAL("Unable to seek to the beginning of the file");
	//		return false;
	//	}
	//
	if (!_pSeekFile->ReadUI32(&streamCapabilitiesSize)) {
		FATAL("Unable to read stream Capabilities Size");
		return false;
	}
	if (streamCapabilitiesSize > 0x01000000) {
		FATAL("Unable to deserialize stream capabilities");
		return false;
	}

	if (!raw.ReadFromFs(*_pSeekFile, streamCapabilitiesSize)) {
		FATAL("Unable to read raw stream Capabilities");
		return false;
	}
	if (!_streamCapabilities.Deserialize(raw, this)) {
		FATAL("Unable to deserialize stream Capabilities. Please delete %s and %s files so they can be regenerated",
				STR(GetName() + "."MEDIA_TYPE_SEEK),
				STR(GetName() + "."MEDIA_TYPE_META));
		return false;
	}

	//4. compute offsets
	_seekBaseOffset = _pSeekFile->Cursor();
	_framesBaseOffset = _seekBaseOffset + 4;


	//5. Compute the optimal window size by reading the biggest frame size
	//from the seek file.
	if (!_pSeekFile->SeekTo(_pSeekFile->Size() - 8)) {
		FATAL("Unable to seek to %"PRIu64" position", _pSeekFile->Cursor() - 8);
		return false;
	}
	uint64_t maxFrameSize = 0;
	if (!_pSeekFile->ReadUI64(&maxFrameSize)) {
		FATAL("Unable to read max frame size");
		return false;
	}
	if (!_pSeekFile->SeekBegin()) {
		FATAL("Unable to seek to beginning of the file");
		return false;
	}

	//3. Open the media file
	uint32_t windowSize = (uint32_t) maxFrameSize * 16;
	windowSize = windowSize == 0 ? MMAP_MAX_WINDOW_SIZE : windowSize;
	windowSize = windowSize < MMAP_MIN_WINDOW_SIZE ? MMAP_MIN_WINDOW_SIZE : windowSize;
	windowSize = (windowSize > MMAP_MAX_WINDOW_SIZE) ? (windowSize / 2) : windowSize;
	_pFile = GetFile(GetName(), windowSize);
	if (_pFile == NULL) {
		FATAL("Unable to initialize file");
		return false;
	}

	//4. Read the frames count from the file
	if (!_pSeekFile->SeekTo(_seekBaseOffset)) {
		FATAL("Unable to seek to _seekBaseOffset: %"PRIu64, _seekBaseOffset);
		return false;
	}
	if (!_pSeekFile->ReadUI32(&_totalFrames)) {
		FATAL("Unable to read the frames count");
		return false;
	}
	_timeToIndexOffset = _framesBaseOffset + _totalFrames * sizeof (MediaFrame);

	//5. Set the client side buffer length
	if ((GetProtocol() != NULL)&&(GetProtocol()->GetApplication() != NULL))
		GetProtocol()->GetApplication()->GetStreamMetadataResolver()->UpdateStats(
			_metadata.mediaFullPath(),
			_metadata.statsFileFullPath(),
			STATS_OPERATION_INCREMENT_OPEN_COUNT, 1);
	_clientSideBufferLength = _metadata.storage().clientSideBuffer();

	//6. Create the timer
	return InitializeTimer(_clientSideBufferLength, timerType, granularity);
}

bool BaseInFileStream::InitializeTimer(int32_t clientSideBufferLength, TimerType timerType,
		uint32_t granularity) {
	//	FINEST("clientSideBufferLength: %"PRId32"; timerType: %s; granularity: %"PRIu32,
	//			clientSideBufferLength,
	//			(timerType == TIMER_TYPE_HIGH_GRANULARITY) ?
	//			"highGranularity"
	//			: ((timerType == TIMER_TYPE_LOW_GRANULARITY) ?
	//			"lowGranularity"
	//			: ((timerType == TIMER_TYPE_NONE) ? "none" : "unknown")
	//			),
	//			granularity);
	if (_pTimer != NULL) {
		FATAL("Timer already initialized");
		return false;
	}

	switch (timerType) {
		case TIMER_TYPE_HIGH_GRANULARITY:
		{
			_pTimer = new InFileStreamTimer(this);
			_pTimer->EnqueueForHighGranularityTimeEvent(granularity);
			_highGranularityTimers = true;
			break;
		}
		case TIMER_TYPE_LOW_GRANULARITY:
		{
			_pTimer = new InFileStreamTimer(this);
			uint32_t value = (uint32_t) ((double) _clientSideBufferLength * 0.6);
			if (value == 0)
				value = 1;
			_pTimer->EnqueueForTimeEvent(value);
			_highGranularityTimers = false;
			break;
		}
		case TIMER_TYPE_NONE:
		{
			_highGranularityTimers = false;
			break;
		}
		default:
		{
			FATAL("Invalid timer type provided");
			return false;
		}
	}

	return true;
}

double BaseInFileStream::GetPosition() {
	return _currentFrame.dts >= 0 ? _currentFrame.dts : 0;
}

bool BaseInFileStream::SignalPlay(double &dts, double &length) {
	//0. fix dts and length
	dts = dts < 0 ? 0 : dts;
	_playLimit = length;
	//FINEST("dts: %.2f; _playLimit: %.2f", dts, _playLimit);
	//TODO: implement the limit playback

	//1. Seek to the correct point
	if (!InternalSeek(dts)) {
		FATAL("Unable to seek to %.02f", dts);
		return false;
	}

	//2. Put the stream in active mode
	_streamingState = FILE_STREAMING_STATE_PLAYING;

	//3. Start the feed reaction
	ReadyForSend();

	//4. Done
	return true;
}

bool BaseInFileStream::SignalPause() {
	//1. Is this already paused
	if (_streamingState != FILE_STREAMING_STATE_PLAYING)
		return true;

	//2. Put the stream in paused mode
	_streamingState = FILE_STREAMING_STATE_PAUSED;

	//3. Done
	return true;
}

bool BaseInFileStream::SignalResume() {
	//1. Is this already active
	if (_streamingState == FILE_STREAMING_STATE_PLAYING)
		return true;

	//2. Put the stream in active mode
	_streamingState = FILE_STREAMING_STATE_PLAYING;

	//3. Start the feed reaction
	ReadyForSend();

	//5. Done
	return true;
}

bool BaseInFileStream::SignalSeek(double &dts) {
	//1. Seek to the correct point
	if (!InternalSeek(dts)) {
		FATAL("Unable to seek to %.02f", dts);
		return false;
	}

	//2. If the stream is active, re-initiate the feed reaction
	if (_streamingState == FILE_STREAMING_STATE_FINISHED) {
		_streamingState = FILE_STREAMING_STATE_PLAYING;
		ReadyForSend();
	}

	//3. Done
	return true;
}

bool BaseInFileStream::SignalStop() {
	//1. Is this already paused
	if (_streamingState != FILE_STREAMING_STATE_PLAYING)
		return true;

	//2. Put the stream in paused mode
	_streamingState = FILE_STREAMING_STATE_PAUSED;

	//3. Done
	return true;
}

void BaseInFileStream::ReadyForSend() {
	bool dataSent = false;
	if (_keepClientBufferFull) {
		do {
			if (!Feed(dataSent)) {
				FATAL("Feed failed");
				if (_pOutStreams != NULL)
					_pOutStreams->info->EnqueueForDelete();
			}
		} while (dataSent);
	} else {
		if (!Feed(dataSent)) {
			FATAL("Feed failed");
			if (_pOutStreams != NULL)
				_pOutStreams->info->EnqueueForDelete();
		}
	}
}

bool BaseInFileStream::InternalSeek(double &dts) {
	//0. We have to send codecs again
	_audioVideoCodecsSent = false;

	//1. Switch to millisecond->FrameIndex table
	if (!_pSeekFile->SeekTo(_timeToIndexOffset)) {
		FATAL("Failed to seek to ms->FrameIndex table");
		return false;
	}

	//2. Read the sampling rate
	uint32_t samplingRate;
	if (!_pSeekFile->ReadUI32(&samplingRate)) {
		FATAL("Unable to read the frames per second");
		return false;
	}

	//3. compute the index in the time2frameindex
	double temp = dts / (double) samplingRate;
	uint32_t tableIndex = 0;
	if ((temp - (uint32_t) temp) != 0)
		tableIndex = (uint32_t) temp + 1;
	else
		tableIndex = (uint32_t) temp;

	//4. Seek to that corresponding index
	if ((_pSeekFile->Cursor() + tableIndex * 4)>(_pSeekFile->Size() - 4)) {
		WARN("SEEK BEYOUND END OF FILE");
		if (!_pSeekFile->SeekEnd()) {
			FATAL("Failed to seek to ms->FrameIndex table");
			return false;
		}
		if (!_pSeekFile->SeekBehind(4)) {
			FATAL("Failed to seek to ms->FrameIndex table");
			return false;
		}
	} else {
		if (!_pSeekFile->SeekAhead(tableIndex * 4)) {
			FATAL("Failed to seek to ms->FrameIndex table");
			return false;
		}
	}

	//5. Read the frame index
	uint32_t frameIndex;
	if (!_pSeekFile->ReadUI32(&frameIndex)) {
		FATAL("Unable to read frame index");
		return false;
	}

	//7. Position the seek file to that particular frame
	if (!_pSeekFile->SeekTo(_framesBaseOffset + frameIndex * sizeof (MediaFrame))) {
		FATAL("Unablt to seek inside seek file");
		return false;
	}

	//8. Read the frame
	if (!_pSeekFile->ReadBuffer((uint8_t *) & _currentFrame, sizeof (MediaFrame))) {
		FATAL("Unable to read frame from seeking file");
		return false;
	}

	//9. update the stream counters
	if (_highGranularityTimers) {
		GETCLOCKS(_startFeedingTime, double);
	} else {
		_startFeedingTime = (double) time(NULL);
	}
	_totalSentTime = 0;
	_currentFrameIndex = frameIndex;
	_totalSentTimeBase = _currentFrame.dts;
	dts = _currentFrame.dts;

	//10. Go back on the frame of interest
	if (!_pSeekFile->SeekTo(_framesBaseOffset + frameIndex * sizeof (MediaFrame))) {
		FATAL("Unablt to seek inside seek file");
		return false;
	}

	_tsChunkSize = 0;
	_tsChunkStart = 0;
	_tsPts = 0;
	_tsDts = 0;

	//11. Done
	return true;
}

bool BaseInFileStream::Feed(bool &dataSent) {
	if (_type == ST_IN_FILE_RTMP)
		return FeedRTMP(dataSent);
	return FeedTS(dataSent);
}

bool BaseInFileStream::SendCodecs() {
	if (_type == ST_IN_FILE_RTMP)
		return SendCodecsRTMP();
	return SendCodecsTS();
}

bool BaseInFileStream::SendCodecsRTMP() {
	//1. Read the first frame
	MediaFrame frame1;
	if (!_pSeekFile->SeekTo(_framesBaseOffset + 0 * sizeof (MediaFrame))) {
		FATAL("Unablt to seek inside seek file");
		return false;
	}
	if (!_pSeekFile->ReadBuffer((uint8_t *) & frame1, sizeof (MediaFrame))) {
		FATAL("Unable to read frame from seeking file");
		return false;
	}

	//2. Read the second frame
	MediaFrame frame2;
	if (!_pSeekFile->SeekTo(_framesBaseOffset + 1 * sizeof (MediaFrame))) {
		FATAL("Unablt to seek inside seek file");
		return false;
	}
	if (!_pSeekFile->ReadBuffer((uint8_t *) & frame2, sizeof (MediaFrame))) {
		FATAL("Unable to read frame from seeking file");
		return false;
	}

	//3. Read the current frame to pickup the timestamp from it
	MediaFrame currentFrame;
	if (!_pSeekFile->SeekTo(_framesBaseOffset + _currentFrameIndex * sizeof (MediaFrame))) {
		FATAL("Unablt to seek inside seek file");
		return false;
	}
	if (!_pSeekFile->ReadBuffer((uint8_t *) & currentFrame, sizeof (MediaFrame))) {
		FATAL("Unable to read frame from seeking file");
		return false;
	}

	//4. Is the first frame a codec setup?
	//If not, the second is not a codec setup for sure
	if (!frame1.isBinaryHeader) {
		_audioVideoCodecsSent = true;
		return true;
	}

	//5. Build the buffer for the first frame
	_tempBuffer.IgnoreAll();
	if (!BuildFrame(_pFile, frame1, _tempBuffer)) {
		FATAL("Unable to build the frame");
		return false;
	}

	//6. Do the feedeng with the first frame
	if (!_pOutStreams->info->FeedData(
			GETIBPOINTER(_tempBuffer), //pData
			GETAVAILABLEBYTESCOUNT(_tempBuffer), //dataLength
			0, //processedLength
			GETAVAILABLEBYTESCOUNT(_tempBuffer), //totalLength
			currentFrame.pts, //pts
			currentFrame.dts, //dts
			frame1.type == MEDIAFRAME_TYPE_AUDIO //isAudio
			)) {
		FATAL("Unable to feed audio data");
		return false;
	}

	//7. Is the second frame a codec setup?
	if (!frame2.isBinaryHeader) {
		_audioVideoCodecsSent = true;
		return true;
	}

	//8. Build the buffer for the second frame
	_tempBuffer.IgnoreAll();
	if (!BuildFrame(_pFile, frame2, _tempBuffer)) {
		FATAL("Unable to build the frame");
		return false;
	}

	//9. Do the feeding with the second frame
	if (!_pOutStreams->info->FeedData(
			GETIBPOINTER(_tempBuffer), //pData
			GETAVAILABLEBYTESCOUNT(_tempBuffer), //dataLength
			0, //processedLength
			GETAVAILABLEBYTESCOUNT(_tempBuffer), //totalLength
			currentFrame.pts, //pts
			currentFrame.dts, //dts
			frame2.type == MEDIAFRAME_TYPE_AUDIO //isAudio
			)) {
		FATAL("Unable to feed audio data");
		return false;
	}

	//10. Done
	_audioVideoCodecsSent = true;
	return true;
}

bool BaseInFileStream::SendCodecsTS() {
	NYIR;
}

bool BaseInFileStream::FeedRTMP(bool &dataSent) {
	dataSent = false;
	//1. Are we in paused state?
	if (_streamingState != FILE_STREAMING_STATE_PLAYING)
		return true;

	//2. First, send audio and video codecs
	if (!_audioVideoCodecsSent) {
		if (!SendCodecs()) {
			FATAL("Unable to send audio codec");
			return false;
		}
	}

	//2. Determine if the client has enough data on the buffer and continue
	//or stay put
	if (_clientSideBufferLength != 0xffffffff) {
		if (_highGranularityTimers) {
			double now;
			GETCLOCKS(now, double);
			double elapsedTime = (now - _startFeedingTime) / (double) CLOCKS_PER_SECOND * 1000.0;
			if ((_totalSentTime - elapsedTime) >= (_clientSideBufferLength * 1000.0)) {
				return true;
			}
		} else {
			int32_t elapsedTime = (int32_t) (time(NULL) - (time_t) _startFeedingTime);
			int32_t totalSentTime = (int32_t) (_totalSentTime / 1000);
			if ((totalSentTime - elapsedTime) >= ((int32_t) _clientSideBufferLength)) {
				return true;
			}
		}
	}

	//3. Test to see if we have sent the last frame
	if (_currentFrameIndex >= _totalFrames) {
		FINEST("Done streaming file");
		_pOutStreams->info->SignalStreamCompleted();
		_streamingState = FILE_STREAMING_STATE_FINISHED;
		return true;
	}

	//FINEST("_totalSentTime: %.2f; _playLimit: %.2f", (double) _totalSentTime, _playLimit);
	if (_playLimit >= 0) {
		if (_playLimit < _totalSentTime) {
			FINEST("Done streaming file");
			_pOutStreams->info->SignalStreamCompleted();
			_streamingState = FILE_STREAMING_STATE_FINISHED;
			return true;
		}
	}

	//4. Read the current frame from the seeking file
	if (!_pSeekFile->SeekTo(_framesBaseOffset + _currentFrameIndex * sizeof (MediaFrame))) {
		FATAL("Unable to seek inside seek file");
		return false;
	}
	if (!_pSeekFile->ReadBuffer((uint8_t *) & _currentFrame, sizeof (_currentFrame))) {
		FATAL("Unable to read frame from seeking file");
		return false;
	}

	//5. Take care of metadata
	if (_currentFrame.type == MEDIAFRAME_TYPE_DATA) {
		_currentFrameIndex++;
		if (!FeedMetaData(_pFile, _currentFrame)) {
			FATAL("Unable to feed metadata");
			return false;
		}
		return Feed(dataSent);
	}

	//6. get our hands on the correct buffer, depending on the frame type: audio or video
	IOBuffer &buffer = _currentFrame.type == MEDIAFRAME_TYPE_AUDIO ? _audioBuffer : _videoBuffer;

	//10. Discard the data
	buffer.IgnoreAll();

	//7. Build the frame
	if (!BuildFrame(_pFile, _currentFrame, buffer)) {
		FATAL("Unable to build the frame");
		return false;
	}

	//8. Compute the timestamp
	_totalSentTime = _currentFrame.dts - _totalSentTimeBase;

	//11. Increment the frame index
	_currentFrameIndex++;

	//9. Do the feeding
	if (!_pOutStreams->info->FeedData(
			GETIBPOINTER(buffer), //pData
			GETAVAILABLEBYTESCOUNT(buffer), //dataLength
			0, //processedLength
			GETAVAILABLEBYTESCOUNT(buffer), //totalLength
			_currentFrame.pts, //pts
			_currentFrame.dts, //dts
			_currentFrame.type == MEDIAFRAME_TYPE_AUDIO //isAudio
			)) {
		FATAL("Unable to feed audio data");
		return false;
	}

	_servedBytesCount += GETAVAILABLEBYTESCOUNT(buffer);

	//12. Done. We either feed again if frame length was 0
	//or just return true
	if (_currentFrame.length == 0) {
		return Feed(dataSent);
	} else {
		dataSent = true;
		return true;
	}
}

bool BaseInFileStream::FeedTS(bool &dataSent) {
	dataSent = false;
	//1. Are we in paused state?
	if (_streamingState != FILE_STREAMING_STATE_PLAYING)
		return true;

	//2. Determine if the client has enough data on the buffer and continue
	//or stay put
	if (_highGranularityTimers) {
		double now;
		GETCLOCKS(now, double);
		double elapsedTime = (now - _startFeedingTime) / (double) CLOCKS_PER_SECOND * 1000.0;
		if ((_totalSentTime - elapsedTime) >= (_clientSideBufferLength * 1000.0)) {
			return true;
		}
	} else {
		int32_t elapsedTime = (int32_t) (time(NULL) - (time_t) _startFeedingTime);
		int32_t totalSentTime = (int32_t) (_totalSentTime / 1000);
		if ((totalSentTime - elapsedTime) >= ((int32_t) _clientSideBufferLength)) {
			return true;
		}
	}

	if (_currentFrameIndex + 1 >= _totalFrames) {
		FINEST("Done streaming file");
		_pOutStreams->info->SignalStreamCompleted();
		_streamingState = FILE_STREAMING_STATE_FINISHED;
		return true;
	}

	//FINEST("_totalSentTime: %.2f; _playLimit: %.2f", (double) _totalSentTime, _playLimit);
	if (_playLimit >= 0) {
		if (_playLimit < _totalSentTime) {
			FINEST("Done streaming file");
			_pOutStreams->info->SignalStreamCompleted();
			_streamingState = FILE_STREAMING_STATE_FINISHED;
			return true;
		}
	}

	if (_tsChunkSize == 0) {
		//4. Read the current frame from the seeking file
		if (!_pSeekFile->SeekTo(_framesBaseOffset + _currentFrameIndex * sizeof (MediaFrame))) {
			FATAL("Unable to seek inside seek file");
			return false;
		}
		if (!_pSeekFile->ReadBuffer((uint8_t *) & _currentFrame, sizeof (_currentFrame))) {
			FATAL("Unable to read frame from seeking file");
			return false;
		}
		//FINEST("CF: %s", STR(_currentFrame));

		_tsChunkStart = _currentFrame.start;
		_tsChunkSize = _currentFrame.start;
		_tsPts = _currentFrame.pts;
		_tsDts = _currentFrame.dts;

		_currentFrameIndex++;

		if (!_pSeekFile->SeekTo(_framesBaseOffset + _currentFrameIndex * sizeof (MediaFrame))) {
			FATAL("Unable to seek inside seek file");
			return false;
		}
		if (!_pSeekFile->ReadBuffer((uint8_t *) & _currentFrame, sizeof (_currentFrame))) {
			FATAL("Unable to read frame from seeking file");
			return false;
		}

		_tsChunkSize = _currentFrame.start - _tsChunkSize;
		//FINEST("_tsChukSize: %"PRIu64, _tsChunkSize);
	}

	if (!_pFile->SeekTo(_tsChunkStart)) {
		FATAL("Unable to seek inside file %s", STR(_pFile->GetPath()));
		return false;
	}
	_videoBuffer.IgnoreAll();
	if (!_videoBuffer.ReadFromFs(*_pFile, (uint32_t) _tsChunkSize)) {
		FATAL("Unable to read data from %s", STR(_pFile->GetPath()));
		return false;
	}

	while (_tsChunkSize != 0) {
		uint32_t size = (GETAVAILABLEBYTESCOUNT(_videoBuffer) > 7 * 188) ? 7 * 188 : GETAVAILABLEBYTESCOUNT(_videoBuffer);
		//FINEST("size: %"PRIu32, size);

		_tsChunkSize -= size;
		_tsChunkStart += size;

		if (!_pOutStreams->info->FeedData(
				GETIBPOINTER(_videoBuffer), //pData
				size, //dataLength
				0, //processedLength
				size, //totalLength
				_tsPts, //pts
				_tsDts, //dts
				MEDIAFRAME_TYPE_VIDEO //isAudio
				)) {
			FATAL("Unable to feed data");
			return false;
		}
		_servedBytesCount += size;

		_videoBuffer.Ignore(size);

		if (_tsChunkSize == 0) {
			_totalSentTime = _currentFrame.dts - _totalSentTimeBase;
			dataSent = true;
		}
	}

	return true;
}
