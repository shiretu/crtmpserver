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


#ifndef _CODECTYPES_H
#define	_CODECTYPES_H

#include "common.h"

#define CODEC_UNKNOWN				MAKE_TAG3('U','N','K')

#define CODEC_VIDEO					MAKE_TAG1('V')
#define CODEC_VIDEO_UNKNOWN			MAKE_TAG4('V','U','N','K')
#define CODEC_VIDEO_PASS_THROUGH	MAKE_TAG3('V','P','T')
#define CODEC_VIDEO_JPEG			MAKE_TAG5('V','J','P','E','G')
#define CODEC_VIDEO_SORENSONH263	MAKE_TAG5('V','S','2','6','3')
#define CODEC_VIDEO_SCREENVIDEO		MAKE_TAG3('V','S','V')
#define CODEC_VIDEO_SCREENVIDEO2	MAKE_TAG4('V','S','V','2')
#define CODEC_VIDEO_VP6				MAKE_TAG4('V','V','P','6')
#define CODEC_VIDEO_VP6ALPHA		MAKE_TAG5('V','V','P','6','A')
#define CODEC_VIDEO_H264			MAKE_TAG5('V','H','2','6','4')

#define CODEC_AUDIO					MAKE_TAG1('A')
#define CODEC_AUDIO_UNKNOWN			MAKE_TAG4('A','U','N','K')
#define CODEC_AUDIO_PASS_THROUGH	MAKE_TAG3('A','P','T')
#define CODEC_AUDIO_PCMLE			MAKE_TAG6('A','P','C','M','L','E')
#define CODEC_AUDIO_PCMBE			MAKE_TAG6('A','P','C','M','B','E')
#define CODEC_AUDIO_NELLYMOSER		MAKE_TAG3('A','N','M')
#define CODEC_AUDIO_MP3				MAKE_TAG4('A','M','P','3')
#define CODEC_AUDIO_AAC				MAKE_TAG4('A','A','A','C')
#define CODEC_AUDIO_G711A			MAKE_TAG6('A','G','7','1','1','A')
#define CODEC_AUDIO_G711U			MAKE_TAG6('A','G','7','1','1','U')
#define CODEC_AUDIO_SPEEX			MAKE_TAG6('A','S','P','E','E','X')

#define CODEC_SUBTITLE				MAKE_TAG1('S')
#define CODEC_SUBTITLE_UNKNOWN		MAKE_TAG4('S','U','N','K')
#define CODEC_SUBTITLE_SRT			MAKE_TAG4('S','S','R','T')
#define CODEC_SUBTITLE_SUB			MAKE_TAG4('S','S','U','B')


#endif	/* _CODECTYPES_H */
