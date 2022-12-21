#include "stdafx.h"

struct VideoDecoderContext
{
	const AVCodec *codec;
	AVCodecContext *av_codec_context;
	AVPacket av_raw_packet;
	AVPacket av_pkt;
	AVFrame *frame;
	AVBufferRef* hw_device_ref;
};

struct ScalerContext
{
	SwsContext* sws_context;
	int source_left;
	int source_top;
	int source_height;
	AVPixelFormat source_pixel_format;
	int scaled_width;
	int scaled_height;
	AVPixelFormat scaled_pixel_format;
};

enum EHwAccel
{
	NO_ACCEL = 0,
	QSV = 1
};

// This does not quite work like avcodec_decode_audio4/avcodec_decode_video2.
// There is the following difference: if you got a frame, you must call
// it again with pkt=NULL. pkt==NULL is treated differently from pkt->size==0
// (pkt==NULL means get more output, pkt->size==0 is a flush/drain packet)
static int decode(AVCodecContext* avctx, AVFrame* frame, int* got_frame, AVPacket* pkt)
{
	int ret;

	*got_frame = 0;

	if (pkt) {
		ret = avcodec_send_packet(avctx, pkt);
		// In particular, we don't expect AVERROR(EAGAIN), because we read all
		// decoded frames with avcodec_receive_frame() until done.
		if (ret < 0 && ret != AVERROR_EOF)
			return ret;
	}

	ret = avcodec_receive_frame(avctx, frame);
	if (ret < 0 && ret != AVERROR(EAGAIN))
		return ret;
	if (ret >= 0)
		*got_frame = 1;

	return 0;
}


static enum AVPixelFormat get_format(AVCodecContext* avctx, const enum AVPixelFormat* pix_fmts)
{
	while (*pix_fmts != AV_PIX_FMT_NONE) {
		if (*pix_fmts == AV_PIX_FMT_QSV) {
			VideoDecoderContext* context = (VideoDecoderContext*)avctx->opaque;
			AVHWFramesContext* frames_ctx;
			AVQSVFramesContext* frames_hwctx;
			int ret;

			/* create a pool of surfaces to be used by the decoder */
			avctx->hw_frames_ctx = av_hwframe_ctx_alloc(context->hw_device_ref);
			if (!avctx->hw_frames_ctx)
				return AV_PIX_FMT_NONE;
			frames_ctx = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
			frames_hwctx = (AVQSVFramesContext*)frames_ctx->hwctx;

			frames_ctx->format = AV_PIX_FMT_QSV;
			frames_ctx->sw_format = avctx->sw_pix_fmt;
			frames_ctx->width = FFALIGN(avctx->coded_width, 32);
			frames_ctx->height = FFALIGN(avctx->coded_height, 32);
			frames_ctx->initial_pool_size = 32;

			frames_hwctx->frame_type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

			ret = av_hwframe_ctx_init(avctx->hw_frames_ctx);
			if (ret < 0)
				return AV_PIX_FMT_NONE;

			return AV_PIX_FMT_QSV;
		}

		pix_fmts++;
	}

	fprintf(stderr, "The QSV pixel format not offered in get_format()\n");

	return AV_PIX_FMT_NONE;
}

int create_video_decoder(int codec_id, int hwAccel, void **handle)
{
	if (!handle)
		return -1;

	auto context = static_cast<VideoDecoderContext *>(av_mallocz(sizeof(VideoDecoderContext)));

	if (!context)
		return -2;

	if (hwAccel == EHwAccel::NO_ACCEL)
	{
		context->codec = avcodec_find_decoder(static_cast<AVCodecID>(codec_id));
	}
	else if (hwAccel == EHwAccel::QSV)
	{
		/* open the hardware device */
		if (av_hwdevice_ctx_create(&context->hw_device_ref, AV_HWDEVICE_TYPE_QSV, "auto", NULL, 0) < 0) {
			remove_video_decoder(context);
			return -3;
		}

		/* initialize the decoder */
		switch (codec_id)
		{
			case AV_CODEC_ID_H264:
				context->codec = avcodec_find_decoder_by_name("h264_qsv");
				break;
			case AV_CODEC_ID_HEVC:
				context->codec = avcodec_find_decoder_by_name("h265_qsv");
				break;
		}				
	}

	if (!context->codec)
	{
		remove_video_decoder(context);
		return -4;
	}

	context->av_codec_context = avcodec_alloc_context3(context->codec);
	if (!context->av_codec_context)
	{
		remove_video_decoder(context);
		return -5;
	}

	if (hwAccel == EHwAccel::QSV)
	{
		context->av_codec_context->codec_id = static_cast<AVCodecID>(codec_id);
		context->av_codec_context->opaque = context;
		context->av_codec_context->get_format = get_format;				

		av_opt_set_int(context->av_codec_context, "refcounted_frames", 1, 0);
	}

	if (avcodec_open2(context->av_codec_context, context->codec, nullptr) < 0)
	{
		remove_video_decoder(context);
		return -6;
	}

	context->frame = av_frame_alloc();
	if (!context->frame)
	{
		remove_video_decoder(context);
		return -7;
	}

	av_init_packet(&context->av_raw_packet);

	*handle = context;

	return 0;
}

int set_video_decoder_extradata(void *handle, void *extradata, int extradataLength)
{
#if _DEBUG
	if (!handle || !extradata || !extradataLength)
		return -1;
#endif

	const auto context = static_cast<VideoDecoderContext *>(handle);

	if (!context->av_codec_context->extradata || context->av_codec_context->extradata_size < extradataLength)
	{
		av_free(context->av_codec_context->extradata);
		context->av_codec_context->extradata = static_cast<uint8_t*>(av_malloc(extradataLength + AV_INPUT_BUFFER_PADDING_SIZE));

		if (!context->av_codec_context->extradata)
			return -2;
	}

	context->av_codec_context->extradata_size = extradataLength;

	memcpy(context->av_codec_context->extradata, extradata, extradataLength);
	memset(context->av_codec_context->extradata + extradataLength, 0, AV_INPUT_BUFFER_PADDING_SIZE);
	
	avcodec_close(context->av_codec_context);
	if (avcodec_open2(context->av_codec_context, context->codec, nullptr) < 0)
		return -3;

	return 0;
}

int decode_video_frame(void *handle, void *rawBuffer, int rawBufferLength, int *frameWidth, int *frameHeight, int *framePixelFormat)
{
#if _DEBUG
	if (!handle || !rawBuffer || !rawBufferLength || !frameWidth || !frameHeight || !framePixelFormat)
		return -1;

	if (reinterpret_cast<uintptr_t>(rawBuffer) % 4 != 0)
		return -2;
#endif

	auto context = static_cast<VideoDecoderContext *>(handle);

	context->av_raw_packet.data = static_cast<uint8_t *>(rawBuffer);
	context->av_raw_packet.size = rawBufferLength;

	int got_frame;

	int ret = decode(context->av_codec_context, context->frame, &got_frame, &context->av_raw_packet);
	if (ret != 0)
		return ret;

	if (got_frame)
	{
		*frameWidth = context->av_codec_context->width;
		*frameHeight = context->av_codec_context->height;
		*framePixelFormat = context->av_codec_context->pix_fmt;
		return 0;
	}

	return -4;

	/*int ret = avcodec_send_packet(context->av_codec_context, &context->av_raw_packet);
	if (ret != 0)
		return -3;*/

	/*int ret = avcodec_receive_frame(context->av_codec_context, context->frame);
	if (ret != 0)
		return ret;*/

	//AVFrame* frame = av_frame_alloc();

	//ret = avcodec_receive_frame(context->av_codec_context, context->frame);
	//if (ret == AVERROR(EAGAIN) || ret < 0)
	//	return ret;

	//if (context->hw_device_ref != NULL)
	//{
	//	/*ret = av_hwframe_transfer_data(context->frame, frame, 0);
	//	if (ret < 0) {
	//		return ret;
	//	}*/
	//}

	///*for (i = 0; i < FF_ARRAY_ELEMS(sw_frame->data) && sw_frame->data; i++)
	//	for (j = 0; j < (sw_frame->height >> (i > 0)); j++)
	//		avio_write(output_ctx, sw_frame->data + j * sw_frame->linesize, sw_frame->width);*/

	///*av_frame_unref(sw_frame);
	//av_frame_unref(frame);*/

 //	*frameWidth = context->av_codec_context->width;
	//*frameHeight = context->av_codec_context->height;
	//*framePixelFormat = context->av_codec_context->pix_fmt;
	//return ret;
}

int scale_decoded_video_frame(void *handle, void *scalerHandle, void *scaledBuffer, int scaledBufferStride)
{
#if _DEBUG
	if (!handle || !scalerHandle || !scaledBuffer)
		return -1;
#endif

	auto context = static_cast<VideoDecoderContext *>(handle);
	const auto scalerContext = static_cast<ScalerContext *>(scalerHandle);

	if (scalerContext->source_top != 0 || scalerContext->source_left != 0)
	{
		const AVPixFmtDescriptor *sourceFmtDesc = av_pix_fmt_desc_get(scalerContext->source_pixel_format);

		if (!sourceFmtDesc)
			return -4;

		const int x_shift = sourceFmtDesc->log2_chroma_w;
		const int y_shift = sourceFmtDesc->log2_chroma_h;
		
		uint8_t *srcData[8];

		srcData[0] = context->frame->data[0] + scalerContext->source_top * context->frame->linesize[0] + scalerContext->source_left;
		srcData[1] = context->frame->data[1] + (scalerContext->source_top >> y_shift) * context->frame->linesize[1] + (scalerContext->source_left >> x_shift);
		srcData[2] = context->frame->data[2] + (scalerContext->source_top >> y_shift) * context->frame->linesize[2] + (scalerContext->source_left >> x_shift);
		srcData[3] = nullptr;
		srcData[4] = nullptr;
		srcData[5] = nullptr;
		srcData[6] = nullptr;
		srcData[7] = nullptr;

		sws_scale(scalerContext->sws_context, srcData, context->frame->linesize, 0,
			scalerContext->source_height, reinterpret_cast<uint8_t **>(&scaledBuffer), &scaledBufferStride);
	}
	else
	{
		sws_scale(scalerContext->sws_context, context->frame->data, context->frame->linesize, 0,
			scalerContext->source_height, reinterpret_cast<uint8_t **>(&scaledBuffer), &scaledBufferStride);
	}

	return 0;
}

void remove_video_decoder(void *handle)
{
	if (!handle)
		return;

	auto context = static_cast<VideoDecoderContext *>(handle);
	
	if (context->av_codec_context)
	{
		av_free(context->av_codec_context->extradata);

		if (context->av_codec_context->hw_device_ctx != NULL)
			av_buffer_unref(&context->av_codec_context->hw_device_ctx);

		avcodec_close(context->av_codec_context);
		av_free(context->av_codec_context);
	}

	av_frame_free(&context->frame);
	av_free(context);
}

int create_video_scaler(int sourceLeft, int sourceTop, int sourceWidth, int sourceHeight, int sourcePixelFormat,
	int scaledWidth, int scaledHeight, int scaledPixelFormat, int quality, void **handle)
{
	if (!handle)
		return -1;

	auto context = static_cast<ScalerContext *>(av_mallocz(sizeof(ScalerContext)));

	if (!context)
		return -2;

	const auto sourceAvPixelFormat = static_cast<AVPixelFormat>(sourcePixelFormat);
	const auto scaledAvPixelFormat = static_cast<AVPixelFormat>(scaledPixelFormat);

	SwsContext *swsContext = sws_getContext(sourceWidth, sourceHeight, sourceAvPixelFormat, scaledWidth, scaledHeight,
		scaledAvPixelFormat, quality, nullptr, nullptr, nullptr);
	
	if (!swsContext)
	{
		remove_video_scaler(context);
		return -3;
	}

	context->sws_context = swsContext;
	context->source_left = sourceLeft;
	context->source_top = sourceTop;
	context->source_height = sourceHeight;
	context->source_pixel_format = sourceAvPixelFormat;
	context->scaled_width = scaledWidth;
	context->scaled_height = scaledHeight;
	context->scaled_pixel_format = scaledAvPixelFormat;

	*handle = context;
	return 0;
}

void remove_video_scaler(void *handle)
{
	if (!handle)
		return;

	const auto context = static_cast<ScalerContext *>(handle);

	sws_freeContext(context->sws_context);
	av_free(context);
}

