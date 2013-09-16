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

#include "streaming/streamcapabilities.h"
#include "streaming/codectypes.h"
#include "streaming/baseinstream.h"

#define STREAM_CAPABILITIES_VERSION MAKE_TAG2('V','9')

//#define DUMP_VAL(name,type,length) if(length!=0) FINEST("%8s %50s: %4.0f; l: %2u; ba: %4u",#type,name,(double)v[name],length,ba.AvailableBits()); else WARN("%8s %50s: %4.0f; l: %2u; ba: %4u",#type,name,(double)v[name],length,ba.AvailableBits());
#define DUMP_VAL(name,type,length)

#define CHECK_BA_LIMITS(name,length) \
if(ba.AvailableBits()<length) { \
	FATAL("Unable to read `"name"` value. Not enough bits. Wanted: %u; Have: %u", \
		(uint32_t)length, ba.AvailableBits()); \
	return false; \
}

#define READ_INT(name,type,length) \
{ \
	CHECK_BA_LIMITS(name,length); \
	v[name]=(type)ba.ReadBits<type>(length); \
	DUMP_VAL(name,type,length); \
}
#define READ_EG(name,type) \
{ \
	uint64_t ___value___=0; \
	if(!ba.ReadExpGolomb(___value___)) { \
		FATAL("Unable to read `"name"` value"); \
		return false; \
	} \
	v[name]=(type)___value___; \
	DUMP_VAL(name,type,0); \
}

#define READ_BOOL(name) \
{ \
	CHECK_BA_LIMITS(name,1); \
	v[name]=(bool)(ba.ReadBits<bool>(1)); \
	DUMP_VAL(name,bool,1); \
}

bool ReadSPSVUIHRD(BitArray &ba, Variant &v) {
	//E.1.2 HRD parameters syntax
	//14496-10.pdf 268/280
	READ_EG("cpb_cnt_minus1", uint64_t);
	READ_INT("bit_rate_scale", uint8_t, 4);
	READ_INT("cpb_size_scale", uint8_t, 4);
	for (uint64_t i = 0; i <= (uint64_t) v["cpb_cnt_minus1"]; i++) {
		uint64_t val = 0;
		if (!ba.ReadExpGolomb(val)) {
			FATAL("Unable to read bit_rate_value_minus1 value");
			return false;
		}
		v["bit_rate_value_minus1"].PushToArray(val);

		if (!ba.ReadExpGolomb(val)) {
			FATAL("Unable to read cpb_size_value_minus1 value");
			return false;
		}
		v["cpb_size_value_minus1"].PushToArray(val);

		CHECK_BA_LIMITS("cbr_flag", 1);
		v["cbr_flag"].PushToArray((bool)ba.ReadBits<bool>(1));
	}
	READ_INT("initial_cpb_removal_delay_length_minus1", uint8_t, 5);
	READ_INT("cpb_removal_delay_length_minus1", uint8_t, 5);
	READ_INT("dpb_output_delay_length_minus1", uint8_t, 5);
	READ_INT("time_offset_length", uint8_t, 5);
	return true;
}

bool ReadSPSVUI(BitArray &ba, Variant &v) {
	//E.1.1 VUI parameters syntax
	//14496-10.pdf 267/280
	READ_BOOL("aspect_ratio_info_present_flag");
	if ((bool)v["aspect_ratio_info_present_flag"]) {
		READ_INT("aspect_ratio_idc", uint8_t, 8);
		if ((uint8_t) v["aspect_ratio_idc"] == 255) {
			READ_INT("sar_width", uint16_t, 16);
			READ_INT("sar_height", uint16_t, 16);
		}
	}
	READ_BOOL("overscan_info_present_flag");
	if ((bool)v["overscan_info_present_flag"])
		READ_BOOL("overscan_appropriate_flag");
	READ_BOOL("video_signal_type_present_flag");
	if ((bool)v["video_signal_type_present_flag"]) {
		READ_INT("video_format", uint8_t, 3);
		READ_BOOL("video_full_range_flag");
		READ_BOOL("colour_description_present_flag");
		if ((bool)v["colour_description_present_flag"]) {
			READ_INT("colour_primaries", uint8_t, 8);
			READ_INT("transfer_characteristics", uint8_t, 8);
			READ_INT("matrix_coefficients", uint8_t, 8);
		}
	}
	READ_BOOL("chroma_loc_info_present_flag");
	if ((bool)v["chroma_loc_info_present_flag"]) {
		READ_EG("chroma_sample_loc_type_top_field", uint64_t);
		READ_EG("chroma_sample_loc_type_bottom_field", uint64_t);
	}
	READ_BOOL("timing_info_present_flag");
	if ((bool)v["timing_info_present_flag"]) {
		READ_INT("num_units_in_tick", uint32_t, 32);
		READ_INT("time_scale", uint32_t, 32);
		READ_BOOL("fixed_frame_rate_flag");
	}
	READ_BOOL("nal_hrd_parameters_present_flag");
	if ((bool)v["nal_hrd_parameters_present_flag"]) {
		if (!ReadSPSVUIHRD(ba, v["nal_hrd"])) {
			FATAL("Unable to read VUIHRD");
			return false;
		}
	}
	READ_BOOL("vcl_hrd_parameters_present_flag");
	if ((bool)v["vcl_hrd_parameters_present_flag"]) {
		if (!ReadSPSVUIHRD(ba, v["vcl_hrd"])) {
			FATAL("Unable to read VUIHRD");
			return false;
		}
	}
	if (((bool)v["nal_hrd_parameters_present_flag"])
			|| ((bool)v["vcl_hrd_parameters_present_flag"]))
		READ_BOOL("low_delay_hrd_flag");
	READ_BOOL("pic_struct_present_flag");
	READ_BOOL("bitstream_restriction_flag");
	if ((bool)v["bitstream_restriction_flag"]) {
		READ_BOOL("motion_vectors_over_pic_boundaries_flag");
		READ_EG("max_bytes_per_pic_denom", uint64_t);
		READ_EG("max_bits_per_mb_denom", uint64_t);
		READ_EG("log2_max_mv_length_horizontal", uint64_t);
		READ_EG("log2_max_mv_length_vertical", uint64_t);
		READ_EG("num_reorder_frames", uint64_t);
		READ_EG("max_dec_frame_buffering", uint64_t);
	}
	return true;
}

bool scaling_list(BitArray &ba, uint8_t sizeOfScalingList) {
	uint32_t nextScale = 8;
	uint32_t lastScale = 8;
	uint64_t delta_scale = 0;
	for (uint8_t j = 0; j < sizeOfScalingList; j++) {
		if (nextScale != 0) {
			if (!ba.ReadExpGolomb(delta_scale))
				return false;
			nextScale = (lastScale + delta_scale + 256) % 256;
		}
		lastScale = (nextScale == 0) ? lastScale : nextScale;
	}
	return true;
}

bool ReadSPS(BitArray &ba, Variant &v) {
	//7.3.2.1 Sequence parameter set RBSP syntax
	//14496-10.pdf 43/280
	READ_INT("profile_idc", uint8_t, 8);
	READ_BOOL("constraint_set0_flag");
	READ_BOOL("constraint_set1_flag");
	READ_BOOL("constraint_set2_flag");
	READ_INT("reserved_zero_5bits", uint8_t, 5);
	READ_INT("level_idc", uint8_t, 8);
	READ_EG("seq_parameter_set_id", uint64_t);
	if ((uint64_t) v["profile_idc"] >= 100) {
		READ_EG("chroma_format_idc", uint64_t);
		if ((uint64_t) v["chroma_format_idc"] == 3)
			READ_BOOL("residual_colour_transform_flag");
		READ_EG("bit_depth_luma_minus8", uint64_t);
		READ_EG("bit_depth_chroma_minus8", uint64_t);
		READ_BOOL("qpprime_y_zero_transform_bypass_flag");
		READ_BOOL("seq_scaling_matrix_present_flag");
		if ((bool)v["seq_scaling_matrix_present_flag"]) {
			for (uint8_t i = 0; i < 8; i++) {
				uint8_t flag = 0;
				CHECK_BA_LIMITS("seq_scaling_list_present_flag", 1);
				flag = ba.ReadBits<uint8_t > (1);
				if (flag) {
					if (i < 6) {
						if (!scaling_list(ba, 16)) {
							FATAL("scaling_list failed");
							return false;
						}
					} else {
						if (!scaling_list(ba, 64)) {
							FATAL("scaling_list failed");
							return false;
						}
					}
				}
			}
		}
	}
	READ_EG("log2_max_frame_num_minus4", uint64_t);
	READ_EG("pic_order_cnt_type", uint64_t);
	if ((uint64_t) v["pic_order_cnt_type"] == 0) {
		READ_EG("log2_max_pic_order_cnt_lsb_minus4", uint64_t);
	} else if ((uint64_t) v["pic_order_cnt_type"] == 1) {
		READ_BOOL("delta_pic_order_always_zero_flag");
		READ_EG("offset_for_non_ref_pic", int64_t);
		READ_EG("offset_for_top_to_bottom_field", int64_t);
		READ_EG("num_ref_frames_in_pic_order_cnt_cycle", uint64_t);
		for (uint64_t i = 0; i < (uint64_t) v["num_ref_frames_in_pic_order_cnt_cycle"]; i++) {
			uint64_t val = 0;
			if (!ba.ReadExpGolomb(val)) {
				FATAL("Unable to read offset_for_ref_frame value");
				return false;
			}
			v["offset_for_ref_frame"].PushToArray((int64_t) val);
		}
	}
	READ_EG("num_ref_frames", uint64_t);
	READ_BOOL("gaps_in_frame_num_value_allowed_flag");
	READ_EG("pic_width_in_mbs_minus1", uint64_t);
	READ_EG("pic_height_in_map_units_minus1", uint64_t);
	READ_BOOL("frame_mbs_only_flag");
	if (!((bool)v["frame_mbs_only_flag"]))
		READ_BOOL("mb_adaptive_frame_field_flag");
	READ_BOOL("direct_8x8_inference_flag");
	READ_BOOL("frame_cropping_flag");
	if ((bool)v["frame_cropping_flag"]) {
		READ_EG("frame_crop_left_offset", uint64_t);
		READ_EG("frame_crop_right_offset", uint64_t);
		READ_EG("frame_crop_top_offset", uint64_t);
		READ_EG("frame_crop_bottom_offset", uint64_t);
	}
	READ_BOOL("vui_parameters_present_flag");
	if ((bool)v["vui_parameters_present_flag"]) {
		if (!ReadSPSVUI(ba, v["vui_parameters"])) {
			FATAL("Unable to read VUI");
			return false;
		}
	}
	return true;
}

bool ReadPPS(BitArray &ba, Variant &v) {
	//7.3.2.2 Picture parameter set RBSP syntax
	//14496-10.pdf 44/280
	READ_EG("pic_parameter_set_id", uint64_t);
	READ_EG("seq_parameter_set_id", uint64_t);
	READ_BOOL("entropy_coding_mode_flag");
	READ_BOOL("pic_order_present_flag");
	READ_EG("num_slice_groups_minus1", int64_t);
	if ((int64_t) v["num_slice_groups_minus1"] > 0) {
		READ_EG("slice_group_map_type", uint64_t);
		if ((int64_t) v["slice_group_map_type"] == 0) {
			for (int64_t i = 0; i < (int64_t) v["num_slice_groups_minus1"]; i++) {
				uint64_t val = 0;
				if (!ba.ReadExpGolomb(val)) {
					FATAL("Unable to read run_length_minus1 value");
					return false;
				}
				v["run_length_minus1"].PushToArray(val);
			}
		} else if ((int64_t) v["slice_group_map_type"] == 2) {
			for (int64_t i = 0; i < (int64_t) v["num_slice_groups_minus1"]; i++) {
				uint64_t val = 0;
				if (!ba.ReadExpGolomb(val)) {
					FATAL("Unable to read top_left value");
					return false;
				}
				v["top_left"].PushToArray(val);

				if (!ba.ReadExpGolomb(val)) {
					FATAL("Unable to read bottom_right value");
					return false;
				}
				v["bottom_right"].PushToArray(val);
			}
		} else if (((int64_t) v["slice_group_map_type"] == 3)
				|| ((int64_t) v["slice_group_map_type"] == 4)
				|| ((int64_t) v["slice_group_map_type"] == 5)) {
			READ_BOOL("slice_group_change_direction_flag");
			READ_EG("slice_group_change_rate_minus1", uint64_t);
		} else if ((int64_t) v["slice_group_map_type"] == 6) {
			READ_EG("pic_size_in_map_units_minus1", uint64_t);
			for (uint64_t i = 0; i <= (uint64_t) v["pic_size_in_map_units_minus1"]; i++) {
				uint64_t val = 0;
				if (!ba.ReadExpGolomb(val)) {
					FATAL("Unable to read slice_group_id value");
					return false;
				}
				v["slice_group_id"].PushToArray((int64_t) val);
			}
		}
	}
	READ_EG("num_ref_idx_l0_active_minus1", uint64_t);
	READ_EG("num_ref_idx_l1_active_minus1", uint64_t);
	READ_BOOL("weighted_pred_flag");
	READ_INT("weighted_bipred_idc", uint8_t, 2);
	READ_EG("pic_init_qp_minus26", int64_t);
	READ_EG("pic_init_qs_minus26", int64_t);
	READ_EG("chroma_qp_index_offset", int64_t);
	READ_BOOL("deblocking_filter_control_present_flag");
	READ_BOOL("constrained_intra_pred_flag");
	READ_BOOL("redundant_pic_cnt_present_flag");
	return true;
}

CodecInfo::CodecInfo() {
	_type = CODEC_UNKNOWN;
	_samplingRate = 0;
	_transferRate = -1;
}

CodecInfo::~CodecInfo() {
	_type = CODEC_UNKNOWN;
	_samplingRate = 0;
	_transferRate = -1;
}

bool CodecInfo::Serialize(IOBuffer & buffer) {
	uint64_t temp64 = EHTONLL(_type);
	buffer.ReadFromBuffer((uint8_t *) & temp64, sizeof (temp64));

	uint32_t temp32 = EHTONL(_samplingRate);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));

	uint8_t tempBuff[sizeof (_transferRate) ];
	EHTONDP(_transferRate, tempBuff);
	buffer.ReadFromBuffer(tempBuff, sizeof (_transferRate));

	return true;
}

bool CodecInfo::Deserialize(IOBuffer & buffer) {
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < (sizeof (_type) + sizeof (_samplingRate) + sizeof (_transferRate))) {
		FATAL("Not enough data to deserialize CodecInfo");
		return false;
	}
	_type = ENTOHLLP(pBuffer);
	_samplingRate = ENTOHLP(pBuffer + sizeof (_type));
	ENTOHDP(pBuffer + sizeof (_type) + sizeof (_samplingRate), _transferRate);
	return buffer.Ignore(sizeof (_type) + sizeof (_samplingRate) + sizeof (_transferRate));
}

void CodecInfo::GetRTMPMetadata(Variant &destination) {
	switch (_type) {
		case CODEC_AUDIO_AAC:
		{
			destination["audiocodecid"] = "mp4a";
			if (_transferRate > 1)
				destination["audiodatarate"] = _transferRate / 1024.0;
			if (_samplingRate > 1)
				destination["audiosamplerate"] = _samplingRate;
			break;
		}
		case CODEC_VIDEO_H264:
		{
			destination["videocodecid"] = "avc1";
			if (_transferRate > 1)
				destination["videodatarate"] = _transferRate / 1024.0;
			break;
		}
		case CODEC_AUDIO_MP3:
		{
			destination["audiocodecid"] = ".mp3";
			if (_transferRate > 1)
				destination["audiodatarate"] = _transferRate / 1024.0;
			if (_samplingRate > 1)
				destination["audiosamplerate"] = _samplingRate;
			break;
		}
		case CODEC_AUDIO_NELLYMOSER:
		{
			destination["audiocodecid"] = "nmos";
			if (_transferRate > 1)
				destination["audiodatarate"] = _transferRate / 1024.0;
			if (_samplingRate > 1)
				destination["audiosamplerate"] = _samplingRate;
			break;
		}
		case CODEC_VIDEO_VP6:
		case CODEC_VIDEO_VP6ALPHA:
		{
			destination["videocodecid"] = "VP62";
			if (_transferRate > 1)
				destination["videodatarate"] = _transferRate / 1024.0;
			break;
		}
		case CODEC_VIDEO_SORENSONH263:
		{
			destination["videocodecid"] = "FLV1";
			if (_transferRate > 1)
				destination["videodatarate"] = _transferRate / 1024.0;
			break;
		}
		case CODEC_VIDEO_SCREENVIDEO:
		case CODEC_VIDEO_SCREENVIDEO2:
		case CODEC_AUDIO_PCMLE:
		case CODEC_AUDIO_PCMBE:
		case CODEC_AUDIO_G711A:
		case CODEC_AUDIO_G711U:
		{
			break;
		}
		case CODEC_AUDIO_SPEEX:
		{
			destination["audiocodecid"] = ".spx";
			if (_transferRate > 1)
				destination["audiodatarate"] = _transferRate / 1024.0;
			if (_samplingRate > 1)
				destination["audiosamplerate"] = _samplingRate;
			break;
		}
		default:
		{
			//WARN("RTMP metadata for codec %s not yet implemented", STR(tagToString(_type)));
			break;
		}
	}
}

CodecInfo::operator string() {
	return format("%s %.3fKHz %.2fKb/s",
			STR(tagToString(_type).substr(1)),
			(double) _samplingRate / 1000.0,
			_transferRate >= 0 ? ((double) _transferRate / 1024.0) : 0.0
			);
}

VideoCodecInfo::VideoCodecInfo()
: CodecInfo() {
	_width = 0;
	_height = 0;
	_frameRateNominator = 0;
	_frameRateDenominator = 0;
}

VideoCodecInfo::~VideoCodecInfo() {
	_width = 0;
	_height = 0;
	_frameRateNominator = 0;
	_frameRateDenominator = 0;
}

double VideoCodecInfo::GetFPS() {
	if ((_frameRateNominator != 0) && (_frameRateDenominator != 0))
		return (double) _frameRateNominator / ((double) _frameRateDenominator * 2.0);
	return 0;
}

bool VideoCodecInfo::Serialize(IOBuffer & buffer) {
	if (!CodecInfo::Serialize(buffer)) {
		FATAL("Unable to serialize CodecInfo");
		return false;
	}
	uint32_t temp32 = EHTONL(_frameRateNominator);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));

	temp32 = EHTONL(_frameRateDenominator);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));

	temp32 = EHTONL(_width);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));

	temp32 = EHTONL(_height);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));

	return true;
}

bool VideoCodecInfo::Deserialize(IOBuffer & buffer) {
	if (!CodecInfo::Deserialize(buffer)) {
		FATAL("Unable to deserialize CodecInfo");
		return false;
	}
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < (sizeof (_width) + sizeof (_width) + sizeof (_frameRateNominator) + sizeof (_frameRateDenominator))) {
		FATAL("Not enough data to deserialize VideoCodecInfo");
		return false;
	}
	_frameRateNominator = ENTOHLP(pBuffer);
	_frameRateDenominator = ENTOHLP(pBuffer + sizeof (_frameRateNominator));
	_width = ENTOHLP(pBuffer + sizeof (_frameRateNominator) + sizeof (_frameRateDenominator));
	_height = ENTOHLP(pBuffer + sizeof (_frameRateNominator) + sizeof (_frameRateDenominator) + sizeof (_width));
	return buffer.Ignore(sizeof (_frameRateNominator) + sizeof (_frameRateDenominator) + sizeof (_width) + sizeof (_height));
}

void VideoCodecInfo::GetRTMPMetadata(Variant &destination) {
	CodecInfo::GetRTMPMetadata(destination);
	if (_width > 0)
		destination["width"] = _width;
	if (_height > 0)
		destination["height"] = _height;
	double fps = 0;
	if ((fps = GetFPS()) != 0) {
		destination["framerate"] = fps;
	}
}

VideoCodecInfo::operator string() {
	return format("%s %"PRIu32"x%"PRIu32" %.2f fps",
			STR(CodecInfo::operator string()),
			_width,
			_height,
			GetFPS());
}

VideoCodecInfoPassThrough::VideoCodecInfoPassThrough()
: VideoCodecInfo() {
	_type = CODEC_VIDEO_PASS_THROUGH;
}

VideoCodecInfoPassThrough::~VideoCodecInfoPassThrough() {
}

bool VideoCodecInfoPassThrough::Init() {
	return true;
}

void VideoCodecInfoPassThrough::GetRTMPMetadata(Variant &destination) {
	//NYI;
}

VideoCodecInfoH264::VideoCodecInfoH264()
: VideoCodecInfo() {
	_level = 0;
	_profile = 0;
	_pSPS = NULL;
	_spsLength = 0;
	_pPPS = NULL;
	_ppsLength = 0;
	_type = CODEC_VIDEO_H264;
}

VideoCodecInfoH264::~VideoCodecInfoH264() {
	_level = 0;
	_profile = 0;
	if (_pSPS != NULL)
		delete[] _pSPS;
	_pSPS = NULL;
	_spsLength = 0;

	if (_pPPS != NULL)
		delete[] _pPPS;
	_pPPS = NULL;
	_ppsLength = 0;
}

bool VideoCodecInfoH264::Serialize(IOBuffer & buffer) {
	if (!VideoCodecInfo::Serialize(buffer)) {
		FATAL("Unable to serialize VideoCodecInfo");
		return false;
	}

	buffer.ReadFromByte(_level);

	buffer.ReadFromByte(_profile);

	uint32_t temp32 = EHTONL(_spsLength);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));

	temp32 = EHTONL(_ppsLength);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));

	buffer.ReadFromBuffer(_pSPS, _spsLength);
	buffer.ReadFromBuffer(_pPPS, _ppsLength);

	return true;
}

bool VideoCodecInfoH264::Deserialize(IOBuffer & buffer) {
	if (!VideoCodecInfo::Deserialize(buffer)) {
		FATAL("Unable to deserialize VideoCodecInfo");
		return false;
	}
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t length = GETAVAILABLEBYTESCOUNT(buffer);

	if (length < (sizeof (_level) + sizeof (_profile))) {
		FATAL("Not enough data to deserialize VideoCodecInfoH264");
		return false;
	}
	_level = pBuffer[0];
	_profile = pBuffer[1];
	buffer.Ignore(sizeof (_level) + sizeof (_profile));
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);

	if (length < (sizeof (_spsLength) + sizeof (_ppsLength))) {
		FATAL("Not enough data to deserialize VideoCodecInfoH264");
		return false;
	}
	_spsLength = ENTOHLP(pBuffer);
	_ppsLength = ENTOHLP(pBuffer + sizeof (_spsLength));
	if (!buffer.Ignore(sizeof (_spsLength) + sizeof (_ppsLength))) {
		FATAL("Unable to deserialize VideoCodecInfoH264");
		return false;
	}
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < (_spsLength + _ppsLength)) {
		FATAL("Not enough data to deserialize VideoCodecInfoH264");
		return false;
	}
	if (_pSPS != NULL) {
		delete[] _pSPS;
	}
	_pSPS = new uint8_t[_spsLength];
	memcpy(_pSPS, pBuffer, _spsLength);
	if (_pPPS != NULL) {
		delete[] _pPPS;
	}
	_pPPS = new uint8_t[_ppsLength];
	memcpy(_pPPS, pBuffer + _spsLength, _ppsLength);
	return buffer.Ignore(_spsLength + _ppsLength);
}

VideoCodecInfoH264::operator string() {
	return format("%s SPS/PPS: %"PRIu32"/%"PRIu32" L/P: %"PRIu8"/%"PRIu8,
			STR(VideoCodecInfo::operator string()),
			_spsLength,
			_ppsLength,
			_level,
			_profile
			);
}

bool VideoCodecInfoH264::Init(uint8_t *pSPS, uint32_t spsLength, uint8_t *pPPS,
		uint32_t ppsLength, uint32_t samplingRate) {
	if ((spsLength < 8)
			|| (spsLength > 65535)
			|| (ppsLength == 0)
			|| (ppsLength > 65535)) {
		FATAL("Invalid SPS/PPS lengths");
		return false;
	}

	_spsLength = spsLength;
	if (_pSPS != NULL) {
		delete[] _pSPS;
	}
	_pSPS = new uint8_t[_spsLength];
	memcpy(_pSPS, pSPS, _spsLength);

	_ppsLength = (uint16_t) ppsLength;
	if (_pPPS != NULL) {
		delete[] _pPPS;
	}
	_pPPS = new uint8_t[_ppsLength];
	memcpy(_pPPS, pPPS, _ppsLength);
	_width = 0;
	_height = 0;
	_transferRate = 0;
	_samplingRate = samplingRate;
	if (_samplingRate == 0)
		_samplingRate = 90000;
	_type = CODEC_VIDEO_H264;

	BitArray spsBa;
	//remove emulation_prevention_three_byte
	for (uint32_t i = 1; i < _spsLength; i++) {
		if (((i + 2)<(_spsLength - 1))
				&& (_pSPS[i + 0] == 0)
				&& (_pSPS[i + 1] == 0)
				&& (_pSPS[i + 2] == 3)) {
			spsBa.ReadFromRepeat(0, 2);
			i += 2;
		} else {
			spsBa.ReadFromRepeat(_pSPS[i], 1);
		}
	}

	Variant temp;
	if (!ReadSPS(spsBa, temp)) {
		FATAL("Unable to parse SPS");
		return false;
	} else {
		temp.Compact();
		bool frame_mbs_only_flag = (bool)temp["frame_mbs_only_flag"];
		_width = ((uint32_t) temp["pic_width_in_mbs_minus1"] + 1)*16;
		_height = ((uint32_t) temp["pic_height_in_map_units_minus1"] + 1)*16 * (frame_mbs_only_flag ? 1 : 2);
		if ((bool)temp["frame_cropping_flag"]) {
			_width -= 2 * (uint32_t) temp["frame_crop_left_offset"] + 2 * (uint32_t) temp["frame_crop_right_offset"];
			_height -= 2 * (uint32_t) temp["frame_crop_top_offset"] + 2 * (uint32_t) temp["frame_crop_bottom_offset"];
		}
		_level = (uint8_t) temp["level_idc"];
		_profile = (uint8_t) temp["profile_idc"];
		if ((temp.HasKeyChain(_V_NUMERIC, true, 2, "vui_parameters", "num_units_in_tick"))
				&& (temp.HasKeyChain(_V_NUMERIC, true, 2, "vui_parameters", "time_scale"))) {
			_frameRateNominator = (uint32_t) temp["vui_parameters"]["time_scale"];
			_frameRateDenominator = (uint32_t) temp["vui_parameters"]["num_units_in_tick"];
		}
	}

	BitArray ppsBa;
	//remove emulation_prevention_three_byte
	for (uint32_t i = 1; i < _ppsLength; i++) {
		if (((i + 2)<(_ppsLength - 1))
				&& (_pPPS[i + 0] == 0)
				&& (_pPPS[i + 1] == 0)
				&& (_pPPS[i + 2] == 3)) {
			ppsBa.ReadFromRepeat(0, 2);
			i += 2;
		} else {
			ppsBa.ReadFromRepeat(_pPPS[i], 1);
		}
	}


	temp.Reset();
	if (!ReadPPS(ppsBa, temp)) {
		FATAL("Unable to partse PPS");
		return false;
	}

	return true;
}

void VideoCodecInfoH264::GetRTMPMetadata(Variant &destination) {
	VideoCodecInfo::GetRTMPMetadata(destination);
	destination["avclevel"] = _level;
	destination["avcprofile"] = _profile;
	//<DOUBLE name="videokeyframe_frequency">5.000</DOUBLE>
}

IOBuffer & VideoCodecInfoH264::GetRTMPRepresentation() {
	if (GETAVAILABLEBYTESCOUNT(_rtmpRepresentation) != 0)
		return _rtmpRepresentation;

	_rtmpRepresentation.ReadFromByte(0x17); //0x10 - key frame; 0x07 - H264_CODEC_ID
	_rtmpRepresentation.ReadFromByte(0); //0: AVC sequence header; 1: AVC NALU; 2: AVC end of sequence
	_rtmpRepresentation.ReadFromByte(0); //CompositionTime
	_rtmpRepresentation.ReadFromByte(0); //CompositionTime
	_rtmpRepresentation.ReadFromByte(0); //CompositionTime
	_rtmpRepresentation.ReadFromByte(1); //version
	_rtmpRepresentation.ReadFromBuffer(_pSPS + 1, 3); //profile,profile compat,level
	_rtmpRepresentation.ReadFromByte(0xff); //6 bits reserved (111111) + 2 bits nal size length - 1 (11)
	_rtmpRepresentation.ReadFromByte(0xe1); //3 bits reserved (111) + 5 bits number of sps (00001)
	uint16_t temp16 = EHTONS((uint16_t) _spsLength);
	_rtmpRepresentation.ReadFromBuffer((uint8_t *) & temp16, 2); //SPS length
	_rtmpRepresentation.ReadFromBuffer(_pSPS, _spsLength);


	_rtmpRepresentation.ReadFromByte(1);
	temp16 = EHTONS((uint16_t) _ppsLength);
	_rtmpRepresentation.ReadFromBuffer((uint8_t *) & temp16, 2); //PPS length
	_rtmpRepresentation.ReadFromBuffer(_pPPS, _ppsLength);

	return _rtmpRepresentation;
}

IOBuffer & VideoCodecInfoH264::GetSPSBuffer() {
	if (GETAVAILABLEBYTESCOUNT(_sps) != 0)
		return _sps;
	_sps.ReadFromBuffer(_pSPS, _spsLength);
	return _sps;
}

IOBuffer & VideoCodecInfoH264::GetPPSBuffer() {
	if (GETAVAILABLEBYTESCOUNT(_pps) != 0)
		return _pps;
	_pps.ReadFromBuffer(_pPPS, _ppsLength);
	return _pps;
}

bool VideoCodecInfoH264::Compare(uint8_t *pSPS, uint32_t spsLength, uint8_t *pPPS,
		uint32_t ppsLength) {
	return (_spsLength == spsLength)
			&&(_ppsLength == ppsLength)
			&&(pSPS != NULL)
			&&(pPPS != NULL)
			&&(_pSPS != NULL)
			&&(_pPPS != NULL)
			&&(memcmp(_pSPS, pSPS, spsLength) == 0)
			&&(memcmp(_pPPS, pPPS, ppsLength) == 0);
}

VideoCodecInfoSorensonH263::VideoCodecInfoSorensonH263()
: VideoCodecInfo() {
	_pHeaders = NULL;
	_length = 0;
	_type = CODEC_VIDEO_SORENSONH263;
}

VideoCodecInfoSorensonH263::~VideoCodecInfoSorensonH263() {
	if (_pHeaders != NULL)
		delete[] _pHeaders;
	_pHeaders = NULL;
	_length = 0;
}

bool VideoCodecInfoSorensonH263::Serialize(IOBuffer & buffer) {
	if (!VideoCodecInfo::Serialize(buffer)) {
		FATAL("Unable to serialize VideoCodecInfo");
		return false;
	}
	uint32_t temp32 = EHTONL(_length);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));
	buffer.ReadFromBuffer(_pHeaders, _length);
	return true;
}

bool VideoCodecInfoSorensonH263::Deserialize(IOBuffer & buffer) {
	if (!VideoCodecInfo::Deserialize(buffer)) {
		FATAL("Unable to deserialize VideoCodecInfo");
		return false;
	}
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < sizeof (_length)) {
		FATAL("Not enough data to deserialize VideoCodecInfoSorensonH263");
		return false;
	}
	_length = ENTOHLP(pBuffer);
	if (!buffer.Ignore(sizeof (_length))) {
		FATAL("Unable to deserialize VideoCodecInfoSorensonH263");
		return false;
	}
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < _length) {
		FATAL("Not enough data to deserialize VideoCodecInfoSorensonH263");
		return false;
	}
	if (_pHeaders != NULL) {
		delete[] _pHeaders;
	}
	_pHeaders = new uint8_t[_length];
	memcpy(_pHeaders, pBuffer, _length);
	return buffer.Ignore(_length);
}

VideoCodecInfoSorensonH263::operator string() {
	return format("%s Headers: %"PRIu32,
			STR(VideoCodecInfo::operator string()),
			_length);
}

bool VideoCodecInfoSorensonH263::Init(uint8_t *pHeaders, uint32_t length) {
	if ((length == 0) || (length > 65535)) {
		FATAL("Invalid headers lengths");
		return false;
	}
	_length = length;
	if (_pHeaders != NULL) {
		delete[] _pHeaders;
	}
	_pHeaders = new uint8_t[_length];
	memcpy(_pHeaders, pHeaders, _length);

	_width = 0;
	_height = 0;
	_transferRate = 0;
	_samplingRate = 90000;
	_type = CODEC_VIDEO_SORENSONH263;

	BitArray ba;
	ba.ReadFromBuffer(pHeaders, length);

	//17 - marker
	//5 - format1
	//8 - picture number/timestamp
	//3 - format2
	//2*8 or 2*16 - width/height depending on the format2

	if (ba.AvailableBits() < 33) {
		FATAL("Not enough bits");
		return false;
	}

	uint32_t marker = ba.ReadBits<uint32_t > (17);
	if (marker != 0x01) {
		FATAL("Invalid marker: %"PRIx32, marker);
		return false;
	}

	uint8_t format1 = ba.ReadBits<uint8_t > (5);
	if ((format1) != 0 && (format1 != 1)) {
		FATAL("Invalid format1: %"PRIx8, format1);
		return false;
	}

	/*uint8_t pictureNumber =*/ ba.ReadBits<uint8_t > (8);
	//	if (pictureNumber != 0) {
	//		WARN("This is not the first picture from a Sorenson H.263 stream: %"PRIx8, pictureNumber);
	//	}

	uint8_t format2 = ba.ReadBits<uint8_t > (3);

	switch (format2) {
		case 0:
			if (ba.AvailableBits() < 16) {
				FATAL("Not enough bits");
				return false;
			}
			_width = ba.ReadBits<uint8_t > (8);
			_height = ba.ReadBits<uint8_t > (8);
			break;
		case 1:
			if (ba.AvailableBits() < 32) {
				FATAL("Not enough bits");
				return false;
			}
			_width = ba.ReadBits<uint16_t > (16);
			_height = ba.ReadBits<uint16_t > (16);
			break;
		case 2:
			_width = 352;
			_height = 288;
			break;
		case 3:
			_width = 176;
			_height = 144;
			break;
		case 4:
			_width = 128;
			_height = 96;
			break;
		case 5:
			_width = 320;
			_height = 240;
			break;
		case 6:
			_width = 160;
			_height = 120;
			break;
		default:
		{
			FATAL("Invalid format2: %"PRIx8, format2);
			return false;
		}
	}

	return true;
}

void VideoCodecInfoSorensonH263::GetRTMPMetadata(Variant &destination) {
	VideoCodecInfo::GetRTMPMetadata(destination);
	//NYI;
}

VideoCodecInfoVP6::VideoCodecInfoVP6()
: VideoCodecInfo() {
	_pHeaders = NULL;
	_length = 0;
	_type = CODEC_VIDEO_VP6;
}

VideoCodecInfoVP6::~VideoCodecInfoVP6() {
	if (_pHeaders != NULL)
		delete[] _pHeaders;
	_pHeaders = NULL;
	_length = 0;
}

bool VideoCodecInfoVP6::Serialize(IOBuffer & buffer) {
	if (!VideoCodecInfo::Serialize(buffer)) {
		FATAL("Unable to serialize VideoCodecInfo");
		return false;
	}
	uint32_t temp32 = EHTONL(_length);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));
	buffer.ReadFromBuffer(_pHeaders, _length);
	return true;
}

bool VideoCodecInfoVP6::Deserialize(IOBuffer & buffer) {
	if (!VideoCodecInfo::Deserialize(buffer)) {
		FATAL("Unable to deserialize VideoCodecInfo");
		return false;
	}
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < sizeof (_length)) {
		FATAL("Not enough data to deserialize VideoCodecInfoVP6");
		return false;
	}
	_length = ENTOHLP(pBuffer);
	if (!buffer.Ignore(sizeof (_length))) {
		FATAL("Unable to deserialize VideoCodecInfoVP6");
		return false;
	}
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < _length) {
		FATAL("Not enough data to deserialize VideoCodecInfoVP6");
		return false;
	}
	if (_pHeaders != NULL) {
		delete[] _pHeaders;
	}
	_pHeaders = new uint8_t[_length];
	memcpy(_pHeaders, pBuffer, _length);
	return buffer.Ignore(_length);
}

VideoCodecInfoVP6::operator string() {
	return format("%s Headers: %"PRIu32,
			STR(VideoCodecInfo::operator string()),
			_length);
}

bool VideoCodecInfoVP6::Init(uint8_t *pHeaders, uint32_t length) {
	if (length != 6) {
		FATAL("Invalid headers lengths");
		return false;
	}
	_length = length;
	if (_pHeaders != NULL) {
		delete[] _pHeaders;
	}
	_pHeaders = new uint8_t[_length];
	memcpy(_pHeaders, pHeaders, _length);

	uint8_t widthAdjust = _pHeaders[0] >> 4;
	uint8_t heightAdjust = _pHeaders[0]&0x0f;

	_width = (_pHeaders[4] << 4) - widthAdjust;
	_height = (_pHeaders[5] << 4) - heightAdjust;
	_transferRate = 0;
	_samplingRate = 90000;
	_type = CODEC_VIDEO_VP6;

	return true;
}

void VideoCodecInfoVP6::GetRTMPMetadata(Variant &destination) {
	VideoCodecInfo::GetRTMPMetadata(destination);
}

AudioCodecInfo::AudioCodecInfo()
: CodecInfo() {
	_channelsCount = 0;
	_bitsPerSample = 0;
	_samplesPerPacket = 0;
}

AudioCodecInfo::~AudioCodecInfo() {
	_channelsCount = 0;
	_bitsPerSample = 0;
	_samplesPerPacket = 0;
}

bool AudioCodecInfo::Serialize(IOBuffer & buffer) {
	if (!CodecInfo::Serialize(buffer)) {
		FATAL("Unable to serialize CodecInfo");
		return false;
	}
	buffer.ReadFromBuffer((uint8_t *) & _channelsCount, sizeof (_channelsCount));
	buffer.ReadFromBuffer((uint8_t *) & _bitsPerSample, sizeof (_bitsPerSample));

	int32_t temp32 = EHTONL(_samplesPerPacket);
	buffer.ReadFromBuffer((uint8_t *) & temp32, sizeof (temp32));

	return true;
}

bool AudioCodecInfo::Deserialize(IOBuffer & buffer) {
	if (!CodecInfo::Deserialize(buffer)) {
		FATAL("Unable to deserialize CodecInfo");
		return false;
	}
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < (sizeof (_channelsCount) + sizeof (_bitsPerSample) + sizeof (_samplesPerPacket))) {
		FATAL("Not enough data to deserialize AudioCodecInfo");
		return false;
	}
	_channelsCount = pBuffer[0];
	_bitsPerSample = pBuffer[1];
	_samplesPerPacket = ENTOHLP(pBuffer + sizeof (_channelsCount) + sizeof (_bitsPerSample));
	return buffer.Ignore(sizeof (_channelsCount) + sizeof (_bitsPerSample) + sizeof (_samplesPerPacket));
}

void AudioCodecInfo::GetRTMPMetadata(Variant &destination) {
	CodecInfo::GetRTMPMetadata(destination);
	if (_channelsCount > 0) {
		destination["audiochannels"] = _channelsCount;
		if (_channelsCount > 1) {
			destination["stereo"] = (bool)true;
		} else {
			destination["stereo"] = (bool)false;
		}
	}
}

AudioCodecInfo::operator string() {
	return format("%s %"PRIu8" channels, %"PRIu8" bits/sample",
			STR(CodecInfo::operator string()),
			_channelsCount,
			_bitsPerSample);
}

AudioCodecInfoPassThrough::AudioCodecInfoPassThrough()
: AudioCodecInfo() {
	_type = CODEC_AUDIO_PASS_THROUGH;
}

AudioCodecInfoPassThrough::~AudioCodecInfoPassThrough() {
}

bool AudioCodecInfoPassThrough::Init() {
	return true;
}

void AudioCodecInfoPassThrough::GetRTMPMetadata(Variant &destination) {
	AudioCodecInfo::GetRTMPMetadata(destination);
	//NYI;
}

AudioCodecInfoAAC::AudioCodecInfoAAC()
: AudioCodecInfo() {
	_audioObjectType = 0;
	_sampleRateIndex = 0;
	_pCodecBytes = NULL;
	_codecBytesLength = 0;
	_type = CODEC_AUDIO_AAC;
}

AudioCodecInfoAAC::~AudioCodecInfoAAC() {
	_audioObjectType = 0;
	_sampleRateIndex = 0;
	if (_pCodecBytes != NULL)
		delete[] _pCodecBytes;
	_pCodecBytes = NULL;
	_codecBytesLength = 0;
}

bool AudioCodecInfoAAC::Serialize(IOBuffer & buffer) {
	if (!AudioCodecInfo::Serialize(buffer)) {
		FATAL("Unable to serialize AudioCodecInfo");
		return false;
	}

	buffer.ReadFromByte(_audioObjectType);
	buffer.ReadFromByte(_sampleRateIndex);
	buffer.ReadFromBuffer((uint8_t *) & _codecBytesLength, sizeof (_codecBytesLength));
	buffer.ReadFromBuffer(_pCodecBytes, _codecBytesLength);

	return true;
}

bool AudioCodecInfoAAC::Deserialize(IOBuffer & buffer) {
	if (!AudioCodecInfo::Deserialize(buffer)) {
		FATAL("Unable to deserialize AudioCodecInfo");
		return false;
	}

	//_audioObjectType
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < sizeof (_audioObjectType)) {
		FATAL("Not enough data to deserialize AudioCodecInfoAAC");
		return false;
	}
	_audioObjectType = pBuffer[0];
	if (!buffer.Ignore(sizeof (_audioObjectType))) {
		FATAL("Unable to deserialize AudioCodecInfoAAC");
		return false;
	}

	//_sampleRateIndex
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < sizeof (_sampleRateIndex)) {
		FATAL("Not enough data to deserialize AudioCodecInfoAAC");
		return false;
	}
	_sampleRateIndex = pBuffer[0];
	if (!buffer.Ignore(sizeof (_codecBytesLength))) {
		FATAL("Unable to deserialize AudioCodecInfoAAC");
		return false;
	}

	//_codecBytesLength
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < sizeof (_codecBytesLength)) {
		FATAL("Not enough data to deserialize AudioCodecInfoAAC");
		return false;
	}
	_codecBytesLength = pBuffer[0];
	if (!buffer.Ignore(sizeof (_codecBytesLength))) {
		FATAL("Unable to deserialize AudioCodecInfoAAC");
		return false;
	}

	//_pCodecBytes
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length < _codecBytesLength) {
		FATAL("Not enough data to deserialize AudioCodecInfoAAC");
		return false;
	}
	if (_pCodecBytes != NULL) {
		delete[] _pCodecBytes;
	}
	_pCodecBytes = new uint8_t[_codecBytesLength];
	memcpy(_pCodecBytes, pBuffer, _codecBytesLength);
	return buffer.Ignore(_codecBytesLength);
}

AudioCodecInfoAAC::operator string() {
	return format("%s codec length: %"PRIu8,
			STR(AudioCodecInfo::operator string()),
			_codecBytesLength);
}

bool AudioCodecInfoAAC::Init(uint8_t *pCodecBytes, uint8_t codecBytesLength,
		bool simple) {
	//http://wiki.multimedia.cx/index.php?title=MP4A#Audio_Specific_Config

	if (codecBytesLength < 2) {
		FATAL("Invalid length: %"PRIu8, codecBytesLength);
		return false;
	}

	//1. Prepare the bit array
	BitArray ba;
	ba.ReadFromBuffer(pCodecBytes, codecBytesLength);

	if (!simple) {
		if (ba.AvailableBits() < 1) {
			FATAL("Not enough bits available for reading AAC config");
			return false;
		}
		if (ba.ReadBits<bool>(1)) {
			if (ba.AvailableBits() < 1) {
				FATAL("Not enough bits available for reading AAC config");
				return false;
			}
			ba.ReadBits<bool>(1);
		}
		if (ba.AvailableBits() < 14) {
			FATAL("Not enough bits available for reading AAC config");
			return false;
		}
		ba.ReadBits<uint16_t > (14);

		_pCodecBytes = new uint8_t[2];
		EHTONSP(_pCodecBytes, ba.PeekBits<uint16_t > (16));
		_codecBytesLength = 2;
	}

	//ISO-IEC-14496-3_2005_MPEG4_Audio page 49/1172
	//2. Read the audio object type
	if (ba.AvailableBits() < 5) {
		FATAL("Not enough bits available for reading AAC config");
		return false;
	}
	_audioObjectType = ba.ReadBits<uint8_t > (5);
	if (_audioObjectType == 31) {
		if (ba.AvailableBits() < 6) {
			FATAL("Not enough bits available for reading AAC config");
			return false;
		}
		_audioObjectType = 32 + ba.ReadBits<uint8_t > (6);
	}

	//3. Read the sample rate index
	if (ba.AvailableBits() < 4) {
		FATAL("Not enough bits available for reading AAC config");
		return false;
	}
	_sampleRateIndex = ba.ReadBits<uint8_t > (4);
	switch (_sampleRateIndex) {
		case 13:
		case 14: //we only have 13 values in the table.
		{
			FATAL("Invalid sample rate: %"PRIu8, _sampleRateIndex);
			return false;
		}
		case 15: //this is a special value telling us the freq directly
		{
			if (ba.AvailableBits() < 24) {
				FATAL("Not enough bits available for reading AAC config");
				return false;
			}
			_samplingRate = ba.ReadBits<uint32_t > (24);
			break;
		}
		default: //get it from the table
		{

			uint32_t rates[] = {
				96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000,
				12000, 11025, 8000, 7350
			};
			_samplingRate = rates[_sampleRateIndex];
			break;
		}
	}

	//4. read the channel configuration index
	if (ba.AvailableBits() < 4) {
		FATAL("Not enough bits available for reading AAC config");
		return false;
	}
	_channelsCount = ba.ReadBits<uint8_t > (4);
	if ((_channelsCount == 0) || (_channelsCount >= 8)) {
		FATAL("Invalid _channelConfigurationIndex: %"PRIu8, _channelsCount);
		return false;
	}

	if (simple) {
		_pCodecBytes = new uint8_t[codecBytesLength];
		memcpy(_pCodecBytes, pCodecBytes, codecBytesLength);
		_codecBytesLength = codecBytesLength;
	}

	//FINEST("%s", STR(IOBuffer::DumpBuffer(_pAAC, _aacLength)));

	return true;
}

AudioCodecInfoNellymoser::AudioCodecInfoNellymoser()
: AudioCodecInfo() {

}

AudioCodecInfoNellymoser::~AudioCodecInfoNellymoser() {

}

bool AudioCodecInfoNellymoser::Init(uint32_t samplingRate, uint8_t channelsCount, uint8_t bitsPerSample) {
	_samplingRate = samplingRate;
	_channelsCount = channelsCount;
	_bitsPerSample = bitsPerSample;
	_type = CODEC_AUDIO_NELLYMOSER;
	return true;
}

void AudioCodecInfoNellymoser::GetRTMPMetadata(Variant &destination) {
	AudioCodecInfo::GetRTMPMetadata(destination);
}

AudioCodecInfoMP3::AudioCodecInfoMP3()
: AudioCodecInfo() {

}

AudioCodecInfoMP3::~AudioCodecInfoMP3() {

}

bool AudioCodecInfoMP3::Init(uint32_t samplingRate, uint8_t channelsCount, uint8_t bitsPerSample) {
	_samplingRate = samplingRate;
	_channelsCount = channelsCount;
	_bitsPerSample = bitsPerSample;
	_type = CODEC_AUDIO_MP3;
	return true;
}

void AudioCodecInfoMP3::GetRTMPMetadata(Variant &destination) {
	AudioCodecInfo::GetRTMPMetadata(destination);
}

IOBuffer & AudioCodecInfoAAC::GetRTMPRepresentation() {
	if (GETAVAILABLEBYTESCOUNT(_rtmpRepresentation) != 0)
		return _rtmpRepresentation;
	_rtmpRepresentation.ReadFromByte(0xaf);
	_rtmpRepresentation.ReadFromByte(0x00);
	_rtmpRepresentation.ReadFromBuffer(_pCodecBytes, _codecBytesLength);
	return _rtmpRepresentation;
}

void AudioCodecInfoAAC::GetRTMPMetadata(Variant &destination) {
	AudioCodecInfo::GetRTMPMetadata(destination);
}

void AudioCodecInfoAAC::GetADTSRepresentation(uint8_t *pDest, uint32_t length) {
	BitArray adts;
	adts.PutBits<uint16_t > (0xFFF, 12); // syncword
	adts.PutBits<uint8_t > (0, 1); // id
	adts.PutBits<uint8_t > (0, 2); // layer
	adts.PutBits<uint8_t > (1, 1); // protection_absent
	adts.PutBits<uint8_t > (_audioObjectType - 1, 2); // profile
	adts.PutBits<uint8_t > (_sampleRateIndex, 4); // sampling_frequency_index
	adts.PutBits<uint8_t > (0, 1); // private
	adts.PutBits<uint8_t > (_channelsCount, 3); // channel_configuration
	adts.PutBits<uint8_t > (0, 1); // original
	adts.PutBits<uint8_t > (0, 1); // home
	adts.PutBits<uint8_t > (0, 1); // copyright_id
	adts.PutBits<uint8_t > (0, 1); // copyright_id_start
	adts.PutBits<uint16_t > ((uint16_t) (length + 7), 13); // aac_frame_length = datalength + 7 byte header
	adts.PutBits<uint16_t > (0x7FF, 11); // adts_buffer_fullness
	adts.PutBits<uint8_t > (0, 2); // num_raw_data_blocks
	memcpy(pDest, GETIBPOINTER(adts), 7);
}

void AudioCodecInfoAAC::UpdateADTSRepresentation(uint8_t *pDest, uint32_t length) {
	//00000000 11111111 22222222 33333333 44444444 55555555 66666666
	//xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxYY YYYYYYYY YYYxxxxx xxxxxxxx
	//                              ---** ******** ***
	uint16_t adtsLength = (uint16_t) ((length + 7)&0x1fff);
	pDest[3] = ((pDest[3]&0xfc) | ((uint8_t) (adtsLength >> 11)));
	pDest[4] = ((uint8_t) ((adtsLength >> 3)&0x00ff));
	pDest[5] = ((pDest[5]&0x1f) | ((uint8_t) ((adtsLength & 0x0007) << 5)));
}

bool AudioCodecInfoAAC::Compare(uint8_t *pCodecBytes, uint8_t codecBytesLength, bool simple) {
	return (simple)
			&&(codecBytesLength == _codecBytesLength)
			&&(pCodecBytes != NULL)
			&&(_pCodecBytes != NULL)
			&&(memcmp(_pCodecBytes, pCodecBytes, codecBytesLength) == 0);
}

StreamCapabilities::StreamCapabilities() {
	_transferRate = -1;
	_pVideoTrack = NULL;
	_pAudioTrack = NULL;
}

StreamCapabilities::~StreamCapabilities() {
	Clear();
}

void StreamCapabilities::Clear() {
	_transferRate = -1;
	ClearVideo();
	ClearAudio();
}

void StreamCapabilities::ClearVideo() {
	if (_pVideoTrack != NULL) {
		delete _pVideoTrack;
		_pVideoTrack = NULL;
	}
	_baseMetadata.Reset();
	_baseMetadata.IsArray(false);
}

void StreamCapabilities::ClearAudio() {
	if (_pAudioTrack != NULL) {
		delete _pAudioTrack;
		_pAudioTrack = NULL;
	}
	_baseMetadata.Reset();
	_baseMetadata.IsArray(false);
}

bool StreamCapabilities::Serialize(IOBuffer & buffer) {
	uint64_t temp64 = EHTONLL(STREAM_CAPABILITIES_VERSION);
	buffer.ReadFromBuffer((uint8_t *) & temp64, sizeof (temp64));

	uint8_t tempBuff[sizeof (_transferRate) ];
	EHTONDP(_transferRate, tempBuff);
	buffer.ReadFromBuffer(tempBuff, sizeof (_transferRate));

	temp64 = (_pVideoTrack != NULL) ? (EHTONLL(1)) : (0);
	buffer.ReadFromBuffer((uint8_t *) & temp64, sizeof (temp64));
	if (_pVideoTrack != NULL) {
		if (!_pVideoTrack->Serialize(buffer)) {
			FATAL("Unable to serialize video info");
			buffer.IgnoreAll();
			return false;
		}
	}

	temp64 = (_pAudioTrack != NULL) ? (EHTONLL(1)) : (0);
	buffer.ReadFromBuffer((uint8_t *) & temp64, sizeof (temp64));
	if (_pAudioTrack != NULL) {
		if (!_pAudioTrack->Serialize(buffer)) {
			FATAL("Unable to serialize audio info");
			buffer.IgnoreAll();
			return false;
		}
	}
	return true;
}

bool StreamCapabilities::Deserialize(string &filePath, BaseInStream *pInStream) {
	return Deserialize(filePath.c_str(), pInStream);
}

bool StreamCapabilities::Deserialize(const char *pFilePath, BaseInStream *pInStream) {
	File file;
	if (!file.Initialize(pFilePath, FILE_OPEN_MODE_READ)) {
		FATAL("Unable to open file %s", pFilePath);
		return false;
	}
	uint32_t capabilitiesSize = 0;
	if (!file.ReadUI32(&capabilitiesSize)) {
		FATAL("Unable to read the size capabilities");
		return false;
	}

	// check if larger than 16 MB
	if (capabilitiesSize > 0x01000000) {
		FATAL("Size capabilities too large");
		return false;
	}

	IOBuffer buffer;
	if (!buffer.ReadFromFs(file, capabilitiesSize)) {
		FATAL("Unable to read data from file");
		return false;
	}

	return Deserialize(buffer, pInStream);
}

bool StreamCapabilities::Deserialize(IOBuffer & buffer, BaseInStream *pInStream) {
	Clear();
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t length = GETAVAILABLEBYTESCOUNT(buffer);

	uint64_t version = 0;
	if (length<sizeof (version)) {
		FATAL("Not enough data to deserialize StreamCapabilities");
		return false;
	}
	version = ENTOHLLP(pBuffer);
	if (version != STREAM_CAPABILITIES_VERSION) {
		FATAL("Invalid stream capabilities version");
		return false;
	}
	buffer.Ignore(sizeof (version));
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);


	if (length<sizeof (_transferRate)) {
		FATAL("Not enough data to deserialize StreamCapabilities");
		return false;
	}
	ENTOHDP(pBuffer, _transferRate);
	buffer.Ignore(sizeof (_transferRate));
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);

	uint64_t count = 0;
	if (length<sizeof (count)) {
		FATAL("Not enough data to deserialize StreamCapabilities");
		return false;
	}
	count = ENTOHLLP(pBuffer);
	if ((count != 0) && (count != 1)) {
		FATAL("Invalid tracks count");
		return false;
	}
	buffer.Ignore(sizeof (count));
	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);

	if (count == 1) {
		do {
			pBuffer = GETIBPOINTER(buffer);
			length = GETAVAILABLEBYTESCOUNT(buffer);
			if (length<sizeof (uint64_t)) {
				FATAL("Not enough data to deserialize StreamCapabilities");
				return false;
			}
			uint64_t type = ENTOHLLP(pBuffer);
			VideoCodecInfo *pTemp = NULL;
			switch (type) {
				case CODEC_VIDEO_PASS_THROUGH:
				{
					pTemp = new VideoCodecInfoPassThrough();
					break;
				}
				case CODEC_VIDEO_SORENSONH263:
				{
					pTemp = new VideoCodecInfoSorensonH263();
					break;
				}
				case CODEC_VIDEO_VP6:
				case CODEC_VIDEO_VP6ALPHA:
				{
					pTemp = new VideoCodecInfoVP6();
					break;
				}
				case CODEC_VIDEO_H264:
				{
					pTemp = new VideoCodecInfoH264();
					break;
				}
				case CODEC_VIDEO_JPEG:
				case CODEC_VIDEO_SCREENVIDEO:
				case CODEC_VIDEO_SCREENVIDEO2:
				default:
				{
					FATAL("Invalid codec type: %016"PRIx64, type);
					return false;
				}
			}
			if (pTemp == NULL) {
				FATAL("Unable to deserialize video codec");
				return false;
			}
			if (!pTemp->Deserialize(buffer)) {
				FATAL("Unable to deserialize VideoCodecInfo");
				delete pTemp;
				return false;
			}
			_pVideoTrack = pTemp;
			if (count != 1) {
				WARN("Multiple video tracks not supported");
				return false;
			}
			//break; commented out because do/while (0)
		} while (0);
	}

	pBuffer = GETIBPOINTER(buffer);
	length = GETAVAILABLEBYTESCOUNT(buffer);
	if (length<sizeof (count)) {
		FATAL("Not enough data to deserialize StreamCapabilities");
		return false;
	}
	count = ENTOHLLP(pBuffer);
	if ((count != 0) && (count != 1)) {
		FATAL("Invalid tracks count");
		return false;
	}
	buffer.Ignore(sizeof (count));
	if (count == 1) {
		do {
			pBuffer = GETIBPOINTER(buffer);
			length = GETAVAILABLEBYTESCOUNT(buffer);
			if (length<sizeof (uint64_t)) {
				FATAL("Not enough data to deserialize StreamCapabilities");
				return false;
			}
			uint64_t type = ENTOHLLP(pBuffer);
			AudioCodecInfo *pTemp = NULL;
			switch (type) {
				case CODEC_AUDIO_PASS_THROUGH:
				{
					pTemp = new AudioCodecInfoPassThrough();
					break;
				}
				case CODEC_AUDIO_NELLYMOSER:
				{
					pTemp = new AudioCodecInfoNellymoser();
					break;
				}
				case CODEC_AUDIO_MP3:
				{
					pTemp = new AudioCodecInfoMP3();
					break;
				}
				case CODEC_AUDIO_AAC:
				{
					pTemp = new AudioCodecInfoAAC();
					break;
				}
				case CODEC_AUDIO_PCMLE:
				case CODEC_AUDIO_PCMBE:
				case CODEC_AUDIO_G711A:
				case CODEC_AUDIO_G711U:
				case CODEC_AUDIO_SPEEX:
				default:
				{
					FATAL("Invalid codec type: %016"PRIx64, type);
					return false;
				}
			}
			if (pTemp == NULL) {
				FATAL("Unable to deserialize audio codec");
				return false;
			}
			if (!pTemp->Deserialize(buffer)) {
				FATAL("Unable to deserialize AudioCodecInfo");
				delete pTemp;
				return false;
			}
			_pAudioTrack = pTemp;
			if (count != 1) {
				WARN("Multiple audio tracks not supported");
				return false;
			}
			//break; commented out because do/while(0)
		} while (0);
	}
	if (pInStream != NULL) {
		pInStream->AudioStreamCapabilitiesChanged(this, NULL, _pAudioTrack);
		if (!pInStream->IsEnqueueForDelete())
			pInStream->VideoStreamCapabilitiesChanged(this, NULL, _pVideoTrack);
	}
	return true;
}

StreamCapabilities::operator string() {
	string result = "VIDEO:\n";
	if (_pVideoTrack != NULL) {
		result += "\t" + _pVideoTrack->operator string() + "\n";
	}
	result += "AUDIO:\n";
	if (_pAudioTrack != NULL) {
		result += "\t" + _pAudioTrack->operator string() + "\n";
	}
	result += format("Transfer rate: %.2fKb/s", GetTransferRate() / 1024.0);
	return result;
}

double StreamCapabilities::GetTransferRate() {
	if (_transferRate >= 0)
		return _transferRate;
	double result = 0;
	if (_pVideoTrack != NULL) {
		if (_pVideoTrack->_transferRate > 0)
			result += _pVideoTrack->_transferRate;
	}
	if (_pAudioTrack != NULL) {
		if (_pAudioTrack->_transferRate > 0)
			result += _pAudioTrack->_transferRate;
	}
	return result;
}

void StreamCapabilities::SetTransferRate(double value) {
	_transferRate = value;
}

uint64_t StreamCapabilities::GetVideoCodecType() {
	if (_pVideoTrack != NULL)
		return _pVideoTrack->_type;
	return CODEC_VIDEO_UNKNOWN;
}

uint64_t StreamCapabilities::GetAudioCodecType() {
	if (_pAudioTrack != NULL)
		return _pAudioTrack->_type;
	return CODEC_AUDIO_UNKNOWN;
}

VideoCodecInfo *StreamCapabilities::GetVideoCodec() {
	return _pVideoTrack;
}

AudioCodecInfo *StreamCapabilities::GetAudioCodec() {
	return _pAudioTrack;
}

void StreamCapabilities::GetRTMPMetadata(Variant &destination) {
	destination = _baseMetadata;
	destination[HTTP_HEADERS_SERVER] = BRANDING_BANNER;
	if (_pAudioTrack != NULL) {
		_pAudioTrack->GetRTMPMetadata(destination);
	}
	if (_pVideoTrack != NULL) {
		_pVideoTrack->GetRTMPMetadata(destination);
	}
	destination["bandwidth"] = (uint32_t) (GetTransferRate() / 1024.0);
}

void StreamCapabilities::SetRTMPMetadata(Variant &baseMetadata) {
	_baseMetadata = baseMetadata;
	if (_baseMetadata != V_MAP) {
		_baseMetadata.Reset();
		_baseMetadata.IsArray(false);
	}
}

VideoCodecInfoPassThrough * StreamCapabilities::AddTrackVideoPassThrough(
		BaseInStream *pInStream) {
	if ((_pVideoTrack != NULL)
			&&(_pVideoTrack->_type == CODEC_VIDEO_PASS_THROUGH)) {
		return (VideoCodecInfoPassThrough*) _pVideoTrack;
	}
	VideoCodecInfoPassThrough *pCodecInfo = new VideoCodecInfoPassThrough();
	if (!pCodecInfo->Init()) {
		FATAL("Unable to initialize VideoCodecInfoPassThrough");
		delete pCodecInfo;
		return NULL;
	}
	VideoCodecInfo *pOld = _pVideoTrack;
	_pVideoTrack = pCodecInfo;
	if (pInStream != NULL)
		pInStream->VideoStreamCapabilitiesChanged(this, pOld, pCodecInfo);
	if (pOld != NULL)
		delete pOld;
	return pCodecInfo;
}

VideoCodecInfoH264* StreamCapabilities::AddTrackVideoH264(uint8_t *pSPS,
		uint32_t spsLength, uint8_t *pPPS, uint32_t ppsLength,
		uint32_t samplingRate, BaseInStream *pInStream) {
	if ((_pVideoTrack != NULL)
			&&(_pVideoTrack->_type == CODEC_VIDEO_H264)
			&&(((VideoCodecInfoH264*) _pVideoTrack)->Compare(pSPS, spsLength, pPPS, ppsLength))) {
		return (VideoCodecInfoH264*) _pVideoTrack;
	}
	VideoCodecInfoH264 *pCodecInfo = new VideoCodecInfoH264();
	if (!pCodecInfo->Init(pSPS, spsLength, pPPS, ppsLength, samplingRate)) {
		FATAL("Unable to initialize VideoCodecInfoH264");
		delete pCodecInfo;
		return NULL;
	}
	VideoCodecInfo *pOld = _pVideoTrack;
	_pVideoTrack = pCodecInfo;
	if (pInStream != NULL)
		pInStream->VideoStreamCapabilitiesChanged(this, pOld, pCodecInfo);
	if (pOld != NULL)
		delete pOld;
	return pCodecInfo;
}

VideoCodecInfoSorensonH263 * StreamCapabilities::AddTrackVideoSorensonH263(
		uint8_t *pData, uint32_t length, BaseInStream *pInStream) {
	if ((_pVideoTrack != NULL)
			&&(_pVideoTrack->_type == CODEC_VIDEO_SORENSONH263)) {
		return (VideoCodecInfoSorensonH263*) _pVideoTrack;
	}
	VideoCodecInfoSorensonH263 *pCodecInfo = new VideoCodecInfoSorensonH263();
	if (!pCodecInfo->Init(pData, length)) {
		FATAL("Unable to initialize VideoCodecInfoSorensonH263");
		delete pCodecInfo;
		return NULL;
	}
	VideoCodecInfo *pOld = _pVideoTrack;
	_pVideoTrack = pCodecInfo;
	if (pInStream != NULL)
		pInStream->VideoStreamCapabilitiesChanged(this, pOld, pCodecInfo);
	if (pOld != NULL)
		delete pOld;
	return pCodecInfo;
}

VideoCodecInfoVP6 * StreamCapabilities::AddTrackVideoVP6(uint8_t *pData,
		uint32_t length, BaseInStream *pInStream) {
	if ((_pVideoTrack != NULL)
			&&(_pVideoTrack->_type == CODEC_VIDEO_VP6)) {
		return (VideoCodecInfoVP6*) _pVideoTrack;
	}
	VideoCodecInfoVP6 *pCodecInfo = new VideoCodecInfoVP6();
	if (!pCodecInfo->Init(pData, length)) {
		FATAL("Unable to initialize VideoCodecInfoVP6");
		delete pCodecInfo;
		return NULL;
	}
	VideoCodecInfo *pOld = _pVideoTrack;
	_pVideoTrack = pCodecInfo;
	if (pInStream != NULL)
		pInStream->VideoStreamCapabilitiesChanged(this, pOld, pCodecInfo);
	if (pOld != NULL)
		delete pOld;
	return pCodecInfo;
}

AudioCodecInfoPassThrough * StreamCapabilities::AddTrackAudioPassThrough(
		BaseInStream *pInStream) {
	if ((_pAudioTrack != NULL)
			&&(_pAudioTrack->_type == CODEC_AUDIO_PASS_THROUGH)) {
		return (AudioCodecInfoPassThrough*) _pAudioTrack;
	}
	AudioCodecInfoPassThrough *pCodecInfo = new AudioCodecInfoPassThrough();
	if (!pCodecInfo->Init()) {
		FATAL("Unable to initialize AudioCodecInfoPassThrough");
		delete pCodecInfo;
		return NULL;
	}
	AudioCodecInfo *pOld = _pAudioTrack;
	_pAudioTrack = pCodecInfo;
	if (pInStream != NULL)
		pInStream->AudioStreamCapabilitiesChanged(this, pOld, pCodecInfo);
	if (pOld != NULL)
		delete pOld;
	return pCodecInfo;
}

AudioCodecInfoAAC* StreamCapabilities::AddTrackAudioAAC(uint8_t *pCodecBytes,
		uint8_t codecBytesLength, bool simple, BaseInStream *pInStream) {
	if ((_pAudioTrack != NULL)
			&&(_pAudioTrack->_type == CODEC_AUDIO_AAC)
			&&(((AudioCodecInfoAAC*) _pAudioTrack)->Compare(pCodecBytes, codecBytesLength, simple))) {
		return (AudioCodecInfoAAC*) _pAudioTrack;
	}
	AudioCodecInfoAAC *pCodecInfo = new AudioCodecInfoAAC();
	if (!pCodecInfo->Init(pCodecBytes, codecBytesLength, simple)) {
		FATAL("Unable to initialize AudioCodecInfoAAC");
		delete pCodecInfo;
		return NULL;
	}
	AudioCodecInfo *pOld = _pAudioTrack;
	_pAudioTrack = pCodecInfo;
	if (pInStream != NULL)
		pInStream->AudioStreamCapabilitiesChanged(this, pOld, pCodecInfo);
	if (pOld != NULL)
		delete pOld;
	return pCodecInfo;
}

AudioCodecInfoNellymoser * StreamCapabilities::AddTrackAudioNellymoser(uint32_t samplingRate,
		uint8_t channelsCount, uint8_t bitsPerSample, BaseInStream *pInStream) {
	if ((_pAudioTrack != NULL)
			&&(_pAudioTrack->_type == CODEC_AUDIO_NELLYMOSER)) {
		return (AudioCodecInfoNellymoser*) _pAudioTrack;
	}
	AudioCodecInfoNellymoser *pCodecInfo = new AudioCodecInfoNellymoser();
	if (!pCodecInfo->Init(samplingRate, channelsCount, bitsPerSample)) {
		FATAL("Unable to initialize AudioCodecInfoNellymoser");
		delete pCodecInfo;
		return NULL;
	}
	AudioCodecInfo *pOld = _pAudioTrack;
	_pAudioTrack = pCodecInfo;
	if (pInStream != NULL)
		pInStream->AudioStreamCapabilitiesChanged(this, pOld, pCodecInfo);
	if (pOld != NULL)
		delete pOld;
	return pCodecInfo;
}

AudioCodecInfoMP3 * StreamCapabilities::AddTrackAudioMP3(uint32_t samplingRate,
		uint8_t channelsCount, uint8_t bitsPerSample, BaseInStream *pInStream) {
	if ((_pAudioTrack != NULL)
			&&(_pAudioTrack->_type == CODEC_AUDIO_MP3)) {
		return (AudioCodecInfoMP3*) _pAudioTrack;
	}
	AudioCodecInfoMP3 *pCodecInfo = new AudioCodecInfoMP3();
	if (!pCodecInfo->Init(samplingRate, channelsCount, bitsPerSample)) {
		FATAL("Unable to initialize AudioCodecInfoNellymoser");
		delete pCodecInfo;
		return NULL;
	}
	AudioCodecInfo *pOld = _pAudioTrack;
	_pAudioTrack = pCodecInfo;
	if (pInStream != NULL)
		pInStream->AudioStreamCapabilitiesChanged(this, pOld, pCodecInfo);
	if (pOld != NULL)
		delete pOld;
	return pCodecInfo;
}
