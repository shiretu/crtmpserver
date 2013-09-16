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

#ifndef _CODECINFO_H
#define	_CODECINFO_H

#include "common.h"

class BaseInStream;

struct CodecInfo {
	uint64_t _type;
	uint32_t _samplingRate;
	double _transferRate;
	CodecInfo();
	virtual ~CodecInfo();
	virtual bool Serialize(IOBuffer & buffer);
	virtual bool Deserialize(IOBuffer & buffer);
	virtual void GetRTMPMetadata(Variant & destination);
	virtual operator string();
};

struct VideoCodecInfo : CodecInfo {
	uint32_t _width;
	uint32_t _height;
	uint32_t _frameRateNominator;
	uint32_t _frameRateDenominator;
	VideoCodecInfo();
	virtual ~VideoCodecInfo();
	double GetFPS();
	virtual bool Serialize(IOBuffer & buffer);
	virtual bool Deserialize(IOBuffer & buffer);
	virtual void GetRTMPMetadata(Variant & destination);
	virtual operator string();
};

struct VideoCodecInfoPassThrough : VideoCodecInfo {
	VideoCodecInfoPassThrough();
	virtual ~VideoCodecInfoPassThrough();
	bool Init();
	virtual void GetRTMPMetadata(Variant & destination);
};

struct VideoCodecInfoH264 : VideoCodecInfo {
	uint8_t _level;
	uint8_t _profile;
	uint8_t *_pSPS;
	uint32_t _spsLength;
	uint8_t *_pPPS;
	uint32_t _ppsLength;
	IOBuffer _rtmpRepresentation;
	IOBuffer _sps;
	IOBuffer _pps;
	VideoCodecInfoH264();
	virtual ~VideoCodecInfoH264();
	virtual bool Serialize(IOBuffer & buffer);
	virtual bool Deserialize(IOBuffer & buffer);
	virtual operator string();
	bool Init(uint8_t *pSPS, uint32_t spsLength, uint8_t *pPPS, uint32_t ppsLength,
			uint32_t samplingRate);
	virtual void GetRTMPMetadata(Variant & destination);
	IOBuffer & GetRTMPRepresentation();
	IOBuffer & GetSPSBuffer();
	IOBuffer & GetPPSBuffer();
	bool Compare(uint8_t *pSPS, uint32_t spsLength, uint8_t *pPPS,
			uint32_t ppsLength);
};

struct VideoCodecInfoSorensonH263 : VideoCodecInfo {
	uint8_t *_pHeaders;
	uint32_t _length;
	VideoCodecInfoSorensonH263();
	virtual ~VideoCodecInfoSorensonH263();
	virtual bool Serialize(IOBuffer & buffer);
	virtual bool Deserialize(IOBuffer & buffer);
	virtual operator string();
	bool Init(uint8_t *pHeaders, uint32_t length);
	virtual void GetRTMPMetadata(Variant & destination);
};

struct VideoCodecInfoVP6 : VideoCodecInfo {
	uint8_t *_pHeaders;
	uint32_t _length;
	VideoCodecInfoVP6();
	virtual ~VideoCodecInfoVP6();
	virtual bool Serialize(IOBuffer & buffer);
	virtual bool Deserialize(IOBuffer & buffer);
	virtual operator string();
	bool Init(uint8_t *pHeaders, uint32_t length);
	virtual void GetRTMPMetadata(Variant & destination);
};

struct AudioCodecInfo : CodecInfo {
	uint8_t _channelsCount;
	uint8_t _bitsPerSample;
	int32_t _samplesPerPacket;
	AudioCodecInfo();
	virtual ~AudioCodecInfo();
	virtual bool Serialize(IOBuffer & buffer);
	virtual bool Deserialize(IOBuffer & buffer);
	virtual void GetRTMPMetadata(Variant & destination);
	virtual operator string();
};

struct AudioCodecInfoPassThrough : AudioCodecInfo {
	AudioCodecInfoPassThrough();
	virtual ~AudioCodecInfoPassThrough();
	bool Init();
	virtual void GetRTMPMetadata(Variant & destination);
};

struct AudioCodecInfoAAC : AudioCodecInfo {
	uint8_t _audioObjectType;
	uint8_t _sampleRateIndex;
	uint8_t *_pCodecBytes;
	uint8_t _codecBytesLength;
	IOBuffer _rtmpRepresentation;
	AudioCodecInfoAAC();
	virtual ~AudioCodecInfoAAC();
	virtual bool Serialize(IOBuffer & buffer);
	virtual bool Deserialize(IOBuffer & buffer);
	virtual operator string();
	bool Init(uint8_t *pCodecBytes, uint8_t codecBytesLength, bool simple);
	IOBuffer & GetRTMPRepresentation();
	virtual void GetRTMPMetadata(Variant & destination);
	void GetADTSRepresentation(uint8_t *pDest, uint32_t length);
	static void UpdateADTSRepresentation(uint8_t *pDest, uint32_t length);
	bool Compare(uint8_t *pCodecBytes, uint8_t codecBytesLength, bool simple);
};

struct AudioCodecInfoNellymoser : AudioCodecInfo {
	AudioCodecInfoNellymoser();
	virtual ~AudioCodecInfoNellymoser();
	bool Init(uint32_t samplingRate, uint8_t channelsCount, uint8_t bitsPerSample);
	virtual void GetRTMPMetadata(Variant & destination);
};

struct AudioCodecInfoMP3 : AudioCodecInfo {
	AudioCodecInfoMP3();
	virtual ~AudioCodecInfoMP3();
	bool Init(uint32_t samplingRate, uint8_t channelsCount, uint8_t bitsPerSample);
	virtual void GetRTMPMetadata(Variant & destination);
};

class StreamCapabilities {
private:
	VideoCodecInfo *_pVideoTrack;
	AudioCodecInfo *_pAudioTrack;
	double _transferRate;
	Variant _baseMetadata;
public:
	StreamCapabilities();
	virtual ~StreamCapabilities();
	void Clear();
	void ClearVideo();
	void ClearAudio();
	virtual bool Serialize(IOBuffer & buffer);
	virtual bool Deserialize(string & filePath, BaseInStream *pInStream);
	virtual bool Deserialize(const char *pFilePath, BaseInStream *pInStream);
	virtual bool Deserialize(IOBuffer & buffer, BaseInStream *pInStream);
	virtual operator string();

	/*!
	 * @brief Returns the detected transfer rate in bits/s. If the returned value is
	 * less than 0, that menas the transfer rate is not available
	 */
	double GetTransferRate();

	/*!
	 * @brief Sets the transfer rate in bits per second. This will override
	 * bitrate detection from the audio and video tracks
	 *
	 * @param value - the value expressed in bits/second
	 */
	void SetTransferRate(double value);

	/*!
	 * @brief Returns the video codec type or CODEC_VIDEO_UNKNOWN if video
	 * not available
	 */
	uint64_t GetVideoCodecType();

	/*!
	 * @brief Returns the audio codec type or CODEC_AUDIO_UNKNOWN if audio
	 * not available
	 */
	uint64_t GetAudioCodecType();

	/*!
	 * @brief Returns the video codec as a pointer to the specified class
	 */
	template<typename T> T * GetVideoCodec() {
		if (_pVideoTrack != NULL)
			return (T *) _pVideoTrack;
		return NULL;
	}

	/*!
	 * @brief Returns the generic video codec
	 */
	VideoCodecInfo *GetVideoCodec();

	/*!
	 * @brief Returns the audio codec as a pointer to the specified class
	 */
	template<typename T> T * GetAudioCodec() {
		if (_pAudioTrack != NULL)
			return (T *) _pAudioTrack;
		return NULL;
	}

	/*!
	 * @brief Returns the generic audio codec
	 */
	AudioCodecInfo *GetAudioCodec();

	/*!
	 * @brief Pupulates RTMP info metadata
	 *
	 * @param destination where the information will be stored
	 */
	void GetRTMPMetadata(Variant &destination);
	void SetRTMPMetadata(Variant &baseMetadata);

	VideoCodecInfoPassThrough * AddTrackVideoPassThrough(
			BaseInStream *pInStream);
	VideoCodecInfoH264 * AddTrackVideoH264(uint8_t *pSPS, uint32_t spsLength,
			uint8_t *pPPS, uint32_t ppsLength, uint32_t samplingRate,
			BaseInStream *pInStream);
	VideoCodecInfoSorensonH263 * AddTrackVideoSorensonH263(uint8_t *pData,
			uint32_t length, BaseInStream *pInStream);
	VideoCodecInfoVP6 * AddTrackVideoVP6(uint8_t *pData, uint32_t length,
			BaseInStream *pInStream);

	AudioCodecInfoPassThrough * AddTrackAudioPassThrough(BaseInStream *pInStream);
	AudioCodecInfoAAC * AddTrackAudioAAC(uint8_t *pCodecBytes,
			uint8_t codecBytesLength, bool simple, BaseInStream *pInStream);
	AudioCodecInfoNellymoser * AddTrackAudioNellymoser(uint32_t samplingRate,
			uint8_t channelsCount, uint8_t bitsPerSample, BaseInStream *pInStream);
	AudioCodecInfoMP3 * AddTrackAudioMP3(uint32_t samplingRate,
			uint8_t channelsCount, uint8_t bitsPerSample, BaseInStream *pInStream);
};

#endif	/* _CODECINFO_H */
