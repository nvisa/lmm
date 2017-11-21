#ifdef CONFIG_FFMPEG_EXTRA

#include "libavcodec/h264.h"

#include <stdio.h>

static void decode_sps_pps(H264Context *h, const uint8_t *data, int size)
{
	int bits_size = size * 8;
	int buffer_size = (bits_size + 7) >> 3;
	h->gb.buffer = data;
	h->gb.size_in_bits = bits_size;
	h->gb.size_in_bits_plus8 = bits_size + 8;
	h->gb.buffer_end = data + buffer_size;
	h->gb.index = 0;
	ff_h264_decode_seq_parameter_set(h, 1);
	//qDebug() << "parsed" << h->sps.vui_parameters_present_flag;
}

float get_h264_frame_rate(const uint8_t *data, int size)
{
	H264Context ctx;
	AVCodecContext cctx;
	cctx.flags2 = 0;
	ctx.avctx = &cctx;
	for (int i = 0; i <= 32; i++)
		ctx.sps_buffers[i] = NULL;
	decode_sps_pps(&ctx, data, size);
	printf("%d\n", ctx.sps_buffers[0]->num_units_in_tick);
	fflush(stdout);
	return 12.5;
}

#endif

