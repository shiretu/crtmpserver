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

#ifdef HAS_PROTOCOL_RTP
#ifndef _OUTNETRTPUDPH264STREAM_H
#define	_OUTNETRTPUDPH264STREAM_H

#include "protocols/rtp/streaming/baseoutnetrtpudpstream.h"

class DLLEXP OutNetRTPUDPH264Stream
: public BaseOutNetRTPUDPStream {
private:
	bool _forceTcp;
	MSGHDR _videoData;
	double _videoSampleRate;
	VideoCodecInfo *_pVideoInfo;
	bool _firstVideoFrame;
	double _lastVideoPts;

	MSGHDR _audioData;
	double _audioSampleRate;
	AudioCodecInfo *_pAudioInfo;
	IOBuffer _auBuffer;
	double _auPts;
	uint32_t _auCount;

	uint32_t _maxRTPPacketSize;
public:
	OutNetRTPUDPH264Stream(BaseProtocol *pProtocol, string name, bool forceTcp);
	virtual ~OutNetRTPUDPH264Stream();
protected:
	virtual bool FinishInitialization(
			GenericProcessDataSetup *pGenericProcessDataSetup);
	virtual bool PushVideoData(IOBuffer &buffer, double pts, double dts,
			bool isKeyFrame);
	virtual bool PushAudioData(IOBuffer &buffer, double pts, double dts);
	virtual bool IsCodecSupported(uint64_t codec);
	virtual void SignalAudioStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, AudioCodecInfo *pOld,
			AudioCodecInfo *pNew);
	virtual void SignalVideoStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
			VideoCodecInfo *pNew);
private:
	virtual void SignalAttachedToInStream();
};


#endif	/* _OUTNETRTPUDPH264STREAM_H */
#endif /* HAS_PROTOCOL_RTP */

