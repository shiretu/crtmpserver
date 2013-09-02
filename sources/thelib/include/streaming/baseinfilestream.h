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


#ifndef _BASEINFILESTREAM_H
#define	_BASEINFILESTREAM_H

#include "streaming/baseinstream.h"
#include "mediaformats/readers/mediaframe.h"
#include "mediaformats/readers/mediafile.h"
#include "mediaformats/readers/streammetadataresolver.h"
#include "protocols/timer/basetimerprotocol.h"

enum TimerType {
	TIMER_TYPE_HIGH_GRANULARITY = 0,
	TIMER_TYPE_LOW_GRANULARITY,
	TIMER_TYPE_NONE
};

/*!
	@class BaseInFileStream
	@brief
 */

class TSFrameReader;

class DLLEXP BaseInFileStream
: public BaseInStream {
private:

	class InFileStreamTimer
	: public BaseTimerProtocol {
	private:
		BaseInFileStream *_pInFileStream;
	public:
		InFileStreamTimer(BaseInFileStream *pInFileStream);
		virtual ~InFileStreamTimer();
		void ResetStream();
		virtual bool TimePeriodElapsed();
	};
	friend class InFileStreamTimer;

	InFileStreamTimer *_pTimer;

	MediaFile *_pSeekFile;
	MediaFile *_pFile;

	//frame info
	uint32_t _totalFrames;
	uint32_t _currentFrameIndex;
	MediaFrame _currentFrame;

	//timing info
	double _totalSentTime;
	double _totalSentTimeBase;
	double _startFeedingTime;

	//buffering info
	uint32_t _clientSideBufferLength;
	IOBuffer _videoBuffer;
	IOBuffer _audioBuffer;

	//current state info
	uint8_t _streamingState;
	bool _audioVideoCodecsSent;

	//seek offsets
	uint64_t _seekBaseOffset;
	uint64_t _framesBaseOffset;
	uint64_t _timeToIndexOffset;

	//stream capabilities
	StreamCapabilities _streamCapabilities;

	//when to stop playback
	double _playLimit;

	//high granularity timers
	bool _highGranularityTimers;

	bool _keepClientBufferFull;

	//Used to compute temporary data where needed. Is not stack safe
	IOBuffer _tempBuffer;
	uint64_t _tsChunkStart;
	uint64_t _tsChunkSize;
	double _tsPts;
	double _tsDts;

	Metadata _metadata;
	uint64_t _servedBytesCount;
public:
	BaseInFileStream(BaseProtocol *pProtocol, uint64_t type, string name);
	virtual ~BaseInFileStream();

	void SetClientSideBuffer(uint32_t value);
	uint32_t GetClientSideBuffer();

	void KeepClientBufferFull(bool value);
	bool KeepClientBufferFull();

	bool StreamCompleted();

	/*!
	  @brief Returns the stream capabilities. Specifically, codec and codec related info
	 */
	virtual StreamCapabilities * GetCapabilities();

	/*!
		@brief This will initialize the stream internally.
		@param clientSideBufferLength - the client side buffer length expressed in seconds
	 */
	virtual bool Initialize(Metadata &metadata, TimerType timerType,
			uint32_t granularity);

	virtual bool InitializeTimer(int32_t clientSideBufferLength, TimerType timerType,
			uint32_t granularity);

	double GetPosition();

	/*!
		@brief Called when a play command was issued
		@param dts - the timestamp where we want to seek before start the feeding process
	 */
	virtual bool SignalPlay(double &dts, double &length);

	/*!
		@brief Called when a pasue command was issued
	 */
	virtual bool SignalPause();

	/*!
		@brief Called when a resume command was issued
	 */
	virtual bool SignalResume();

	/*!
		@brief Called when a seek command was issued
		@param dts
	 */
	virtual bool SignalSeek(double &dts);

	/*!
		@brief Called when a stop command was issued
	 */
	virtual bool SignalStop();

	/*!
		@brief This is called by the framework. The networking layer signaled the availability for sending data
	 */
	virtual void ReadyForSend();

protected:
	virtual bool BuildFrame(MediaFile *pFile, MediaFrame &mediaFrame,
			IOBuffer &buffer) = 0;
	virtual bool FeedMetaData(MediaFile *pFile, MediaFrame &mediaFrame) = 0;
private:
	/*!
		@brief This will seek to the specified point in time.
		@param dts - the timestamp where we want to seek before start the feeding process
	 */
	bool InternalSeek(double &dts);

public:
	/*!
		@brief This is the function that will actually do the feeding.
		@discussion It is called by the framework and it must deliver one frame at a time to all subscribers
	 */

	virtual bool Feed(bool &dataSent);
private:
	/*!
		@brief This function will ensure that the codec packets are sent. Also it preserves the current timings and frame index
	 */
	bool SendCodecs();
	bool SendCodecsRTMP();
	bool SendCodecsTS();
	virtual bool FeedRTMP(bool &dataSent);
	virtual bool FeedTS(bool &dataSent);
};

#endif	/* _BASEINFILESTREAM_H */

