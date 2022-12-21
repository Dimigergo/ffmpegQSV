// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <Windows.h>

extern "C"
{
	#include <libavcodec/avcodec.h>

	#include "libavformat/avformat.h"

	#include <libavutil/opt.h>	
	#include <libavutil/channel_layout.h>
	#include <libavutil/common.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/samplefmt.h>
	#include "libavutil/buffer.h"
	#include "libavutil/error.h"
	#include "libavutil/hwcontext.h"
	#include "libavutil/hwcontext_qsv.h"
	#include "libavutil/mem.h"

	#include <libswscale/swscale.h>
	#include <libswresample/swresample.h>

	#include <mfx/mfxvideo.h>
}

#include "export.h"