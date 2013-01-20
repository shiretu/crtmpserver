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

#ifdef HAS_PROTOCOL_RTMP
#ifndef _OUTFILERTMPFLVSTREAM_H
#define	_OUTFILERTMPFLVSTREAM_H

#include "streaming/baseoutfilestream.h"

class DLLEXP OutFileRTMPFLVStream
: public BaseOutFileStream {
private:
	File _file;
	double _timeBase;
	IOBuffer _audioBuffer;
	IOBuffer _videoBuffer;
	uint32_t _prevTagSize;
	string _filename;
public:
	OutFileRTMPFLVStream(BaseProtocol *pProtocol, string name, string filename);

	virtual ~OutFileRTMPFLVStream();
	void Initialize();
	virtual bool SignalPlay(double &dts, double &length);
	virtual bool SignalPause();
	virtual bool SignalResume();
	virtual bool SignalSeek(double &dts);
	virtual bool SignalStop();
	virtual bool FeedData(uint8_t *pData, uint32_t dataLength,
			uint32_t processedLength, uint32_t totalLength,
			double pts, double dts, bool isAudio);
	virtual bool IsCompatibleWithType(uint64_t type);
	virtual void SignalAttachedToInStream();
	virtual void SignalDetachedFromInStream();
	virtual void SignalStreamCompleted();
	virtual void SignalAudioStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, AudioCodecInfo *pOld,
			AudioCodecInfo *pNew);
	virtual void SignalVideoStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
			VideoCodecInfo *pNew);
protected:
	virtual bool PushVideoData(IOBuffer &buffer, double pts, double dts,
			bool isKeyFrame);
	virtual bool PushAudioData(IOBuffer &buffer, double pts, double dts);
	virtual bool IsCodecSupported(uint64_t codec);
};


#endif	/* _OUTFILERTMPFLVSTREAM_H */

#endif /* HAS_PROTOCOL_RTMP */

