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


#ifndef _BASEOUTSTREAM_H
#define	_BASEOUTSTREAM_H

#include "streaming/basestream.h"

class BaseInStream;

enum NALUMarkerType {
	NALU_MARKER_TYPE_NONE = 0,
	NALU_MARKER_TYPE_0001,
	NALU_MARKER_TYPE_SIZE
};

struct GenericProcessDataSetup {

	/*!
	 * @brief video setup
	 */
	union {

		/*!
		 * @brief AVC setup
		 */
		struct {
			/*!
			 * @brief It will insert a NALU marker like this:<br>
			 * NALU_MARKER_TYPE_NONE - no marker is inserted<br>
			 * NALU_MARKER_TYPE_0001 - the marker has the value of 0x00000001<br>
			 * NALU_MARKER_TYPE_SIZE - the marker has the value equal with the
			 * size of the NALU as a 4 bytes unsigned integer in network order<br>
			 */
			NALUMarkerType _naluMarkerType;

			/*!
			 * @brief If true, a PD NALU will be inserted in front of each and every
			 * frame. Useful when TS output is needed. It should be false (no PD) when
			 * RTMP/RTSP output is required
			 */
			bool _insertPDNALU;

			/*!
			 * @brief If true, the standard h264 RTMP header will be inserted in the frame
			 */
			bool _insertRTMPPayloadHeader;

			/*!
			 * @brief If true, SPS/PPS will be inserted before IDR
			 */
			bool _insertSPSPPSBeforeIDR;

			/*!
			 * @brief If true, it will aggregate all the NALS with the same timestamp
			 * on the same packet
			 */
			bool _aggregateNALU;

			/*!
			 * @brief bulk initialization of flags rather than setting them one
			 * by one. Preferred method. Easier to track changes
			 */
			void Init(NALUMarkerType naluMarkerType, bool insertPDNALU,
					bool insertRTMPPayloadHeader, bool insertSPSPPSBeforeIDR,
					bool aggregateNALU) {
				_naluMarkerType = naluMarkerType;
				_insertPDNALU = insertPDNALU;
				_insertRTMPPayloadHeader = insertRTMPPayloadHeader;
				_insertSPSPPSBeforeIDR = insertSPSPPSBeforeIDR;
				_aggregateNALU = aggregateNALU;
			}
		} avc;
	} video;

	/*!
	 * @brief audio setup
	 */
	union {

		/*!
		 * @brief AAC setup
		 */
		struct {
			/*!
			 * @brief If true, the standard ADTS header will be inserted in the frame
			 */
			bool _insertADTSHeader;

			/*!
			 * @brief If true, the standard AAC RTMP header will be inserted in the frame
			 */
			bool _insertRTMPPayloadHeader;
		} aac;
	} audio;

	/*!
	 * @brief The time base which is going to be used for all pts/dts. If the
	 * value is less than 0, no time base is going to be applied. Otherwise,
	 * the specified time base is going to be applied. Expressed in milliseconds
	 */
	double _timeBase;

	/*!
	 * @brief the maximum value of the frame size. if 0, the maximum value is
	 * considered infinite
	 */
	uint32_t _maxFrameSize;

	/*!
	 * @brief This value is first computed from the audio track detection and also
	 * from the supported audio codecs. When FinishInitialization, the inheritor
	 * has a chance to put it on false (manually disabling the track).
	 */
	bool _hasAudio;

	/*!
	 * @brief This value is first computed from the video track detection and also
	 * from the supported video codecs. When FinishInitialization, the inheritor
	 * has a chance to put it on false (manually disabling the track).
	 */
	bool _hasVideo;

	/*!
	 * @brief This value is the video codec id detected from the input stream
	 * This is a read-only value.
	 */
	uint64_t _videoCodec;

	/*!
	 * @brief This value is the audio codec id detected from the input stream
	 * This is a read-only value.
	 */
	uint64_t _audioCodec;

	/*!
	 * @brief The input stream capabilities
	 */
	StreamCapabilities *_pStreamCapabilities;
};

class DLLEXP BaseOutStream
: public BaseStream {
private:
	bool _canCallDetachedFromInStream;
	string _aliasName;
	uint64_t _inStreamType;

	//member variables used by GenericProcessData member function

	//audio
	IOBuffer _audioBucket;
	IOBuffer _audioFrame;
	uint8_t _adtsHeader[7];
	uint8_t _adtsHeaderCache[7];
	double _lastAudioTimestamp;

	//video
	IOBuffer _videoBucket;
	IOBuffer _videoFrame;
	bool _isKeyFrame;
	double _lastVideoPts;
	double _lastVideoDts;

	double _zeroTimeBase;
	GenericProcessDataSetup _genericProcessDataSetup;
	double _maxWaitDts;
protected:
	BaseInStream *_pInStream;
public:
	BaseOutStream(BaseProtocol *pProtocol, uint64_t type, string name);
	virtual ~BaseOutStream();

	void SetAliasName(string &aliasName);

	/*!
		@brief Returns the stream capabilities. Specifically, codec and codec related info
	 */
	virtual StreamCapabilities * GetCapabilities();

	/*!
		@brief The networking layer signaled the availability for sending data
	 */
	virtual void ReadyForSend();

	/*!
		@brief Links an in-stream to this stream
		@param pInStream - the in-stream where we want to attach
		@param reverseLink - if true, pInStream::Link will be called internally this is used to break the infinite calls.
	 */
	virtual bool Link(BaseInStream *pInStream, bool reverseLink = true);

	/*!
		@brief Unlinks an in-stream to this stream
		@param reverseUnLink - if true, pInStream::UnLink will be called internally this is used to break the infinite calls
	 */
	virtual bool UnLink(bool reverseUnLink = true);

	/*!
		@brief Returns true if this stream is linked to an inbound stream. Otherwise, returns false
	 */
	bool IsLinked();

	/*!
		@brief Returns the feeder of this stream
	 */
	BaseInStream *GetInStream();

	/*!
		@brief  This will return information about the stream
		@param info
	 */
	virtual void GetStats(Variant &info, uint32_t namespaceId = 0);

	/*!
		@brief This will start the feeding process
		@param dts - the timestamp where we want to seek before start the feeding process\
		@param length
	 */
	virtual bool Play(double dts, double length);

	/*!
		@brief This will pause the feeding process
	 */
	virtual bool Pause();

	/*!
		@brief This will resume the feeding process
	 */
	virtual bool Resume();

	/*!
		@brief This will seek to the specified point in time.
		@param dts
	 */
	virtual bool Seek(double dts);

	/*!
		@brief This will stop the feeding process
	 */
	virtual bool Stop();

	/*!
		@brief Called after the link is complete
	 */
	virtual void SignalAttachedToInStream() = 0;

	/*!
		@brief Called after the link is broken
	 */
	virtual void SignalDetachedFromInStream() = 0;

	/*!
		@brief Called when the feeder finished the work
	 */
	virtual void SignalStreamCompleted() = 0;

	virtual void SignalAudioStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, AudioCodecInfo *pOld,
			AudioCodecInfo *pNew) = 0;
	virtual void SignalVideoStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
			VideoCodecInfo *pNew) = 0;
protected:
	/*!
	 * @brief This function can be used to split the audio and video data
	 * into meaningful parts. Only works with H264 and AAC content
	 *
	 * @param pData - the pointer to the chunk of data
	 *
	 * @param dataLength - the size of pData in bytes
	 *
	 * @param processedLength - how many bytes of the entire frame were
	 * processed so far
	 *
	 * @param totalLength - the size in bytes of the entire frame
	 *
	 * @param pts - presentation timestamp
	 *
	 * @param dts - decoding timestamp
	 *
	 * @param isAudio - if true, the packet contains audio data. Otherwise it
	 * contains video data
	 *
	 * @return should return true for success or false for errors
	 *
	 * @discussion dataLength+processedLength==totalLength at all times
	 */
	bool GenericProcessData(uint8_t *pData, uint32_t dataLength,
			uint32_t processedLength, uint32_t totalLength,
			double pts, double dts, bool isAudio);

	virtual void GenericSignalAudioStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, AudioCodecInfo *pOld,
			AudioCodecInfo *pNew);
	virtual void GenericSignalVideoStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
			VideoCodecInfo *pNew);

	/*!
	 * @brief This function is called when generic parsing is used. It offers
	 * the inheritors the opportunity to do other initializations.
	 */
	virtual bool FinishInitialization(
			GenericProcessDataSetup *pGenericProcessDataSetup);

	/*!
	 * @brief This function will be called by the GenericProcessData function
	 * when a complete video frame is available
	 *
	 * @param buffer - contains the frame formatted according to
	 * _genericProcessDataSetup structure
	 *
	 * @param pts - presentation timestamp
	 *
	 * @param dts - decoding timestamp
	 *
	 * @param isKeyFrame - if true, the frame is an I frame. Otherwise is a P frame
	 *
	 * @return should return true for success or false for errors
	 *
	 * @discussion The receiver of this call should do what is appropriate with
	 * the buffer and return true or false. buffer will always contain a
	 * complete video frame. The video frame contains all the necessary NAL
	 * units to properly decode the frame on the target. It is formatted
	 * according to _genericProcessDataSetup structure
	 */
	virtual bool PushVideoData(IOBuffer &buffer, double pts, double dts, bool isKeyFrame) = 0;

	/*!
	 * @brief This function will be called by the GenericProcessData function
	 * when a complete audio frame is available
	 *
	 * @param buffer - contains the frame formatted according to
	 * _genericProcessDataSetup structure
	 *
	 * @param pts - presentation timestamp
	 *
	 * @param dts - decoding timestamp
	 *
	 * @return should return true for success or false for errors
	 *
	 * @discussion The receiver of this call should do what is appropriate with
	 * the buffer and return true or false. buffer will always contain a
	 * complete audio frame. It is formatted according to
	 * _genericProcessDataSetup structure
	 */
	virtual bool PushAudioData(IOBuffer &buffer, double pts, double dts) = 0;

	/*!
	 * @brief This function will be called by the framework to see if the output
	 * stream supports the target codec.
	 *
	 * @return should return true if the codec is supported, otherwise it should
	 * return false. When false is returned, the track is deactivated on output
	 */
	virtual bool IsCodecSupported(uint64_t codec) = 0;
private:
	void GenericStreamCapabilitiesChanged();
	/*!
	 * @brief This function is used internally to reset the state of the stream.
	 * Called from UnLink(), constructor and destructor
	 */
	void Reset();

	/*!
	 * @brief Used internally by GenericProcessData to validate the stream capabilities
	 * and check if it contains H264/AAC
	 *
	 * @param dts - current frame dts timestamp
	 *
	 * @discussion When the input stream is of type RTMP (file, network or liveflv)
	 * than the framework will wait for the codecs no longer than _maxWaitDts
	 */
	bool ValidateCodecs(double dts);

	/*
	 * Used internally to insert various regions of the payload. They use
	 * _genericProcessDataSetup structure to determine how the parts are put in
	 */
	void InsertVideoNALUMarker(uint32_t naluSize);
	void InsertVideoRTMPPayloadHeader(uint32_t cts);
	void MarkVideoRTMPPayloadHeaderKeyFrame();
	void InsertVideoPDNALU();
	void InsertVideoSPSPPSBeforeIDR();
	void InsertAudioADTSHeader(uint32_t length);
	void InsertAudioRTMPPayloadHeader();

	/*
	 * Will process H264 content coming from an RTMP source and call PushVideoData
	 */
	bool ProcessH264FromRTMP(uint8_t *pBuffer, uint32_t length, double pts, double dts);

	/*
	 * Will process H264 content coming from a TS or RTP source and call PushVideoData
	 */
	bool ProcessH264FromTS(uint8_t *pBuffer, uint32_t length, double pts, double dts);

	/*
	 * Will process AAC content coming from an RTMP source and call PushAudioData
	 */
	bool ProcessAACFromRTMP(uint8_t *pBuffer, uint32_t length, double pts, double dts);

	/*
	 * Will process MP3 content coming from an RTMP source and call PushAudioData
	 */
	bool ProcessMP3FromRTMP(uint8_t *pBuffer, uint32_t length, double pts, double dts);

	/*
	 * Will process AAC content coming from a TS or RTP source and call PushAudioData
	 */
	bool ProcessAACFromTS(uint8_t *pBuffer, uint32_t length, double pts, double dts);

	/*
	 * Will process MP3 content coming from a TS or RTP source and call PushAudioData
	 */
	bool ProcessMP3FromTS(uint8_t *pBuffer, uint32_t length, double pts, double dts);
};

#endif	/* _BASEOUTBOUNDSTREAM_H */

