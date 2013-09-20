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
#include "protocols/rtmp/streaming/infilertmpstream.h"
#include "mediaformats/readers/basemediadocument.h"
#include "mediaformats/readers/flv/flvdocument.h"
#include "mediaformats/readers/mp3/mp3document.h"
#include "mediaformats/readers/mp4/mp4document.h"
#include "protocols/rtmp/basertmpprotocol.h"
#include "streaming/baseoutstream.h"
#include "protocols/baseprotocol.h"
#include "protocols/rtmp/streaming/baseoutnetrtmpstream.h"
#include "streaming/streamstypes.h"
#include "protocols/rtmp/messagefactories/messagefactories.h"
#include "protocols/rtmp/basertmpprotocol.h"
#include "protocols/rtmp/streaming/baseoutnetrtmpstream.h"
#include "streaming/codectypes.h"

InFileRTMPStream::BaseBuilder::BaseBuilder() {

}

InFileRTMPStream::BaseBuilder::~BaseBuilder() {

}

InFileRTMPStream::AVCBuilder::AVCBuilder() {
	_videoCodecHeaderInit[0] = 0x17;
	_videoCodecHeaderInit[1] = 0;
	_videoCodecHeaderInit[2] = 0;
	_videoCodecHeaderInit[3] = 0;
	_videoCodecHeaderInit[4] = 0;

	_videoCodecHeaderKeyFrame[0] = 0x17;
	_videoCodecHeaderKeyFrame[1] = 1;

	_videoCodecHeader[0] = 0x27;
	_videoCodecHeader[1] = 1;
}

InFileRTMPStream::AVCBuilder::~AVCBuilder() {

}

bool InFileRTMPStream::AVCBuilder::BuildFrame(MediaFile* pFile, MediaFrame& mediaFrame, IOBuffer& buffer) {
	if (mediaFrame.isBinaryHeader) {
		buffer.ReadFromBuffer(_videoCodecHeaderInit, sizeof (_videoCodecHeaderInit));
	} else {
		if (mediaFrame.isKeyFrame) {
			// video key frame
			buffer.ReadFromBuffer(_videoCodecHeaderKeyFrame, sizeof (_videoCodecHeaderKeyFrame));
		} else {
			//video normal frame
			buffer.ReadFromBuffer(_videoCodecHeader, sizeof (_videoCodecHeader));
		}
		uint32_t cts = (EHTONL(((uint32_t) mediaFrame.cts) & 0x00ffffff)) >> 8;
		buffer.ReadFromBuffer((uint8_t *) & cts, 3);
	}

	if (!pFile->SeekTo(mediaFrame.start)) {
		FATAL("Unable to seek to position %"PRIu64, mediaFrame.start);
		return false;
	}

	if (!buffer.ReadFromFs(*pFile, (uint32_t) mediaFrame.length)) {
		FATAL("Unable to read %"PRIu64" bytes from offset %"PRIu64, mediaFrame.length, mediaFrame.start);
		return false;
	}

	return true;
}

InFileRTMPStream::AACBuilder::AACBuilder() {
	_audioCodecHeaderInit[0] = 0xaf;
	_audioCodecHeaderInit[1] = 0;
	_audioCodecHeader[0] = 0xaf;
	_audioCodecHeader[1] = 0x01;
}

InFileRTMPStream::AACBuilder::~AACBuilder() {

}

bool InFileRTMPStream::AACBuilder::BuildFrame(MediaFile* pFile, MediaFrame& mediaFrame, IOBuffer& buffer) {
	//1. add the binary header
	if (mediaFrame.isBinaryHeader) {
		buffer.ReadFromBuffer(_audioCodecHeaderInit, sizeof (_audioCodecHeaderInit));
	} else {
		buffer.ReadFromBuffer(_audioCodecHeader, sizeof (_audioCodecHeader));
	}

	//2. Seek into the data file at the correct position
	if (!pFile->SeekTo(mediaFrame.start)) {
		FATAL("Unable to seek to position %"PRIu64, mediaFrame.start);
		return false;
	}

	//3. Read the data
	if (!buffer.ReadFromFs(*pFile, (uint32_t) mediaFrame.length)) {
		FATAL("Unable to read %"PRIu64" bytes from offset %"PRIu64, mediaFrame.length, mediaFrame.start);
		return false;
	}

	return true;
}

InFileRTMPStream::MP3Builder::MP3Builder() {

}

InFileRTMPStream::MP3Builder::~MP3Builder() {

}

bool InFileRTMPStream::MP3Builder::BuildFrame(MediaFile *pFile,
		MediaFrame &mediaFrame, IOBuffer &buffer) {
	buffer.ReadFromRepeat(0x2f, 1);

	//2. Seek into the data file at the correct position
	if (!pFile->SeekTo(mediaFrame.start)) {
		FATAL("Unable to seek to position %"PRIu64, mediaFrame.start);
		return false;
	}

	//3. Read the data
	if (!buffer.ReadFromFs(*pFile, (uint32_t) mediaFrame.length)) {
		FATAL("Unable to read %"PRIu64" bytes from offset %"PRIu64, mediaFrame.length, mediaFrame.start);
		return false;
	}

	return true;
}

InFileRTMPStream::PassThroughBuilder::PassThroughBuilder() {

}

InFileRTMPStream::PassThroughBuilder::~PassThroughBuilder() {

}

bool InFileRTMPStream::PassThroughBuilder::BuildFrame(MediaFile *pFile,
		MediaFrame &mediaFrame, IOBuffer &buffer) {
	//1. Seek into the data file at the correct position
	if (!pFile->SeekTo(mediaFrame.start)) {
		FATAL("Unable to seek to position %"PRIu64, mediaFrame.start);
		return false;
	}

	//2. Read the data
	if (!buffer.ReadFromFs(*pFile, (uint32_t) mediaFrame.length)) {
		FATAL("Unable to read %"PRIu64" bytes from offset %"PRIu64, mediaFrame.length, mediaFrame.start);
		return false;
	}

	//3. Done
	return true;
}

InFileRTMPStream::InFileRTMPStream(BaseProtocol *pProtocol, uint64_t type, string name)
: BaseInFileStream(pProtocol, type, name) {
	_chunkSize = 4 * 1024 * 1024;
	_pAudioBuilder = NULL;
	_pVideoBuilder = NULL;
}

InFileRTMPStream::~InFileRTMPStream() {
	if (_pAudioBuilder != NULL) {
		delete _pAudioBuilder;
		_pAudioBuilder = NULL;
	}
	if (_pVideoBuilder != NULL) {
		delete _pVideoBuilder;
		_pVideoBuilder = NULL;
	}
}

bool InFileRTMPStream::Initialize(Metadata &metadata, TimerType timerType,
		uint32_t granularity) {
	//1. Base init
	if (!BaseInFileStream::Initialize(metadata, timerType, granularity)) {
		FATAL("Unable to initialize stream");
		return false;
	}

	//2. Get stream capabilities
	StreamCapabilities *pCapabilities = GetCapabilities();
	if (pCapabilities == NULL) {
		FATAL("Invalid stream capabilities");
		return false;
	}

	pCapabilities->SetRTMPMetadata(_completeMetadata.publicMetadata());

	//3. Create the video builder
	uint64_t videoCodec = pCapabilities->GetVideoCodecType();
	if ((videoCodec != 0)
			&& (videoCodec != CODEC_VIDEO_UNKNOWN)
			&& (videoCodec != CODEC_VIDEO_H264)
			&& (videoCodec != CODEC_VIDEO_PASS_THROUGH)) {
		FATAL("Invalid video stream capabilities: %s", STR(tagToString(videoCodec)));
		return false;
	}
	if (videoCodec == CODEC_VIDEO_H264) {
		_pVideoBuilder = new AVCBuilder();
	} else if (videoCodec == CODEC_VIDEO_PASS_THROUGH) {
		_pVideoBuilder = new PassThroughBuilder();
	}

	//4. Create the audio builder
	uint64_t audioCodec = pCapabilities->GetAudioCodecType();
	if ((audioCodec != 0)
			&& (audioCodec != CODEC_AUDIO_UNKNOWN)
			&& (audioCodec != CODEC_AUDIO_AAC)
			&& (audioCodec != CODEC_AUDIO_MP3)
			&& (audioCodec != CODEC_AUDIO_PASS_THROUGH)) {
		FATAL("Invalid audio stream capabilities: %s", STR(tagToString(audioCodec)));
		return false;
	}
	if (audioCodec == CODEC_AUDIO_AAC) {
		_pAudioBuilder = new AACBuilder();
	} else if (audioCodec == CODEC_AUDIO_MP3) {
		_pAudioBuilder = new MP3Builder();
	} else if (audioCodec == CODEC_AUDIO_PASS_THROUGH) {
		_pAudioBuilder = new PassThroughBuilder();
	}
	return true;
}

bool InFileRTMPStream::FeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	ASSERT("Operation not supported");
	return false;
}

bool InFileRTMPStream::IsCompatibleWithType(uint64_t type) {
	return TAG_KIND_OF(type, ST_OUT_NET_RTMP)
			|| TAG_KIND_OF(type, ST_OUT_NET_RTP)
			;
}

uint32_t InFileRTMPStream::GetChunkSize() {
	return _chunkSize;
}

InFileRTMPStream *InFileRTMPStream::GetInstance(BaseRTMPProtocol *pRTMPProtocol,
		StreamsManager *pStreamsManager, Metadata &metadata) {
	InFileRTMPStream *pResult = NULL;

	string type = metadata.type();
	if (type == MEDIA_TYPE_FLV
			|| type == MEDIA_TYPE_MP3
			|| type == MEDIA_TYPE_MP4) {
		pResult = new InFileRTMPStream((BaseProtocol *) pRTMPProtocol,
				ST_IN_FILE_RTMP, metadata.mediaFullPath());
	} else {
		FATAL("File type not supported yet. Metadata:\n%s",
				STR(metadata.ToString()));
	}

	if (pResult != NULL) {
		if (!pResult->SetStreamsManager(pStreamsManager)) {
			FATAL("Unable to set the streams manager");
			delete pResult;
			pResult = NULL;
			return NULL;
		}
		pResult->SetCompleteMetadata(metadata);
	}

	return pResult;
}

void InFileRTMPStream::SetCompleteMetadata(Metadata &completeMetadata) {
	_completeMetadata = completeMetadata;
}

Metadata &InFileRTMPStream::GetCompleteMetadata() {
	return _completeMetadata;
}

void InFileRTMPStream::SignalOutStreamAttached(BaseOutStream *pOutStream) {
	//2. Set a big chunk size on the corresponding connection
	if (TAG_KIND_OF(pOutStream->GetType(), ST_OUT_NET_RTMP)) {
		((BaseRTMPProtocol *) pOutStream->GetProtocol())->TrySetOutboundChunkSize(_chunkSize);
		((BaseOutNetRTMPStream *) pOutStream)->SetFeederChunkSize(_chunkSize);
		((BaseOutNetRTMPStream *) pOutStream)->CanDropFrames(false);
	}
}

void InFileRTMPStream::SignalOutStreamDetached(BaseOutStream *pOutStream) {
}

bool InFileRTMPStream::BuildFrame(MediaFile *pFile, MediaFrame &mediaFrame,
		IOBuffer &buffer) {
	switch (mediaFrame.type) {
		case MEDIAFRAME_TYPE_AUDIO:
			if (_pAudioBuilder != NULL)
				return _pAudioBuilder->BuildFrame(pFile, mediaFrame, buffer);
			return true;
		case MEDIAFRAME_TYPE_VIDEO:
			if (_pVideoBuilder != NULL)
				return _pVideoBuilder->BuildFrame(pFile, mediaFrame, buffer);
			return true;
		default:
			return true;
	}
}

bool InFileRTMPStream::FeedMetaData(MediaFile *pFile, MediaFrame &mediaFrame) {
	if ((_pProtocol == NULL)
			|| ((_pProtocol->GetType() != PT_INBOUND_RTMP)
			&&(_pProtocol->GetType() != PT_OUTBOUND_RTMP)))
		return true;
	//1. Seek into the data file at the correct position
	if (!pFile->SeekTo(mediaFrame.start)) {
		FATAL("Unable to seek to position %"PRIu64, mediaFrame.start);
		return false;
	}

	//2. Read the data
	_metadataBuffer.IgnoreAll();
	if (!_metadataBuffer.ReadFromFs(*pFile, (uint32_t) mediaFrame.length)) {
		FATAL("Unable to read %"PRIu64" bytes from offset %"PRIu64, mediaFrame.length, mediaFrame.start);
		return false;
	}

	//3. Parse the metadata
	_metadataName = "";
	_metadataParameters.Reset();

	_tempVariant.Reset();
	if (!_amfSerializer.Read(_metadataBuffer, _tempVariant)) {
		WARN("Unable to read metadata");
		return true;
	}
	if (_tempVariant != V_STRING) {
		WARN("Unable to read metadata");
		return true;
	}
	_metadataName = ((string) _tempVariant);

	while (GETAVAILABLEBYTESCOUNT(_metadataBuffer) > 0) {
		_tempVariant.Reset();
		if (!_amfSerializer.Read(_metadataBuffer, _tempVariant)) {
			WARN("Unable to read metadata");
			return true;
		}
		_metadataParameters.PushToArray(_tempVariant);
	}

	Variant message = GenericMessageFactory::GetNotify(
			((BaseOutNetRTMPStream *) _pOutStreams->info)->GetCommandsChannelId(),
			((BaseOutNetRTMPStream *) _pOutStreams->info)->GetRTMPStreamId(),
			mediaFrame.dts,
			true,
			_metadataName,
			_metadataParameters);

	//5. Send it
	return ((BaseRTMPProtocol *) _pProtocol)->SendMessage(message);
}
#endif /* HAS_PROTOCOL_RTMP */

