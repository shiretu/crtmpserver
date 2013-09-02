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

#ifndef _OUTFILEFLV_H
#define	_OUTFILEFLV_H

#include "recording/baseoutrecording.h"

class BaseOutRecording;

class DLLEXP OutFileFLV
: public BaseOutRecording {
private:
	File *_pFile;
	uint32_t _prevTagSize;
	uint8_t _tagHeader[11];
	double _chunkLength;
	bool _waitForIDR;
	double _lastVideoDts;
	double _lastAudioDts;
	uint32_t _chunkCount;
	Variant _metadata;
	IOBuffer _buff;
public:
	OutFileFLV(BaseProtocol *pProtocol, string name, Variant &settings);
	static OutFileFLV* GetInstance(BaseClientApplication *pApplication,
			string name, Variant &settings);
	virtual ~OutFileFLV();
protected:
	virtual bool FinishInitialization(
			GenericProcessDataSetup *pGenericProcessDataSetup);
	virtual bool PushVideoData(IOBuffer &buffer, double pts, double dts, bool isKeyFrame);
	virtual bool PushAudioData(IOBuffer &buffer, double pts, double dts);
	virtual bool IsCodecSupported(uint64_t codec);
private:
	bool WriteFLVHeader(bool hasAudio, bool hasVideo);
	bool WriteFLVMetaData();
	bool WriteFLVCodecAudio(AudioCodecInfoAAC *pInfoAudio);
	bool WriteFLVCodecVideo(VideoCodecInfoH264 *pInfoVideo);
	bool InitializeFLVFile(GenericProcessDataSetup *pGenericProcessDataSetup);
	bool WriteMetaData(GenericProcessDataSetup *pGenericProcessDataSetup);
	bool WriteCodecSetupBytes(GenericProcessDataSetup *pGenericProcessDataSetup);
	void UpdateDuration();
	bool SplitFile();
};

#endif	/* _OUTFILEFLV_H */
