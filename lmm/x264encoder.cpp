#include "x264encoder.h"

#include <lmm/debug.h>
#include <lmm/streamtime.h>

extern "C" {
	#include <x264.h>
}

#include <errno.h>
#include <linux/videodev2.h>

struct x264EncoderPriv {
	x264_picture_t *pic;
	x264_picture_t picout;
	x264_t *handle;
	x264_param_t param;

	int w;
	int h;
	int stride;
	int pixelFormat;
};

x264Encoder::x264Encoder(QObject *parent) :
	BaseLmmElement(parent)
{
	priv = new x264EncoderPriv;
	priv->pic = new x264_picture_t;
	priv->w = 640;
	priv->h = 360;
	priv->stride = priv->w * 1;
	priv->pixelFormat = X264_CSP_I420;
}

int x264Encoder::start()
{
	if (x264_param_default_preset(&priv->param, "medium", NULL) < 0)
		return -EINVAL;
	priv->param.i_csp = priv->pixelFormat;
	priv->param.i_width  = priv->w;
	priv->param.i_height = priv->h;
	priv->param.b_vfr_input = 0;
	priv->param.i_fps_num = 25 * 1000;
	priv->param.i_fps_den = 1000;
	priv->param.i_timebase_num = 25;
	priv->param.i_timebase_den = 1;
	priv->param.b_repeat_headers = 1;
	priv->param.b_annexb = 1;
	priv->param.i_keyint_max = 25;

	/* rc - CBR at the moment */
	priv->param.rc.i_rc_method = X264_RC_CRF;
	priv->param.rc.i_bitrate = 4000;
	priv->param.rc.i_vbv_max_bitrate = 4000;
	priv->param.rc.i_vbv_buffer_size = priv->param.rc.i_vbv_max_bitrate * 2;

	/* Apply profile restrictions. */
	if (x264_param_apply_profile(&priv->param, "main") < 0)
		return -EINVAL;

	x264_picture_init(priv->pic);
	priv->pic->img.i_csp = priv->pixelFormat;
	if (priv->pixelFormat == X264_CSP_I420) {
		priv->pic->img.i_plane = 3;
		priv->pic->img.i_stride[0] = priv->stride;
		priv->pic->img.i_stride[1] = priv->stride / 2;
		priv->pic->img.i_stride[2] = priv->stride / 2;
	} else if (priv->pixelFormat == X264_CSP_NV12) {
		priv->pic->img.i_plane = 3;
		priv->pic->img.i_stride[0] = priv->stride;
		priv->pic->img.i_stride[1] = priv->stride;
		priv->pic->img.i_stride[2] = priv->stride;
	} else
		return -EINVAL;
	if (x264_picture_alloc(&priv->picout, priv->param.i_csp, priv->param.i_width, priv->param.i_height) < 0)
		return -ENOMEM;

	priv->handle = x264_encoder_open(&priv->param);
	if (!priv->handle)
		return -EINVAL;
	return BaseLmmElement::start();
}

int x264Encoder::stop()
{
	return BaseLmmElement::stop();
}

int x264Encoder::setVideoResolution(const QSize &sz)
{
	int pfmt = 0;
	if (priv) {
		pfmt = priv->pixelFormat;
		if (priv->pic)
			delete priv->pic;
		delete priv;
	}
	priv = new x264EncoderPriv;
	priv->pic = new x264_picture_t;
	priv->w = sz.width();
	priv->h = sz.height();
	priv->stride = priv->w * 1;
	priv->pixelFormat = pfmt;

	return 0;
}

int x264Encoder::setPixelFormat(int fmt)
{
	if (fmt == V4L2_PIX_FMT_NV12)
		priv->pixelFormat = X264_CSP_NV12;
	else if (fmt == V4L2_PIX_FMT_YUV420)
		priv->pixelFormat = X264_CSP_I420;
	else
		return -EINVAL;
	return 0;
}

int x264Encoder::processBuffer(const RawBuffer &buf)
{
	x264_nal_t *nal;
	int i_nal;

	priv->pic->img.plane[0] = (uchar *)buf.constData();
	priv->pic->img.plane[1] = (uchar *)buf.constData() + priv->w * priv->h;
	priv->pic->img.plane[2] = (uchar *)buf.constData() + priv->w * priv->h + priv->w * priv->h / 4;

	x264_encoder_encode(priv->handle, &nal, &i_nal, priv->pic, &priv->picout);

	QList<RawBuffer> list;
	for (int i = 0; i < i_nal; i++) {
		RawBuffer outb("video/x-h264", nal[i].i_payload);
		outb.setParameters(buf.constPars());
		outb.pars()->frameType = priv->picout.i_type;
		outb.pars()->h264NalType = nal[i].i_type;
		outb.pars()->encodeTime = streamTime->getCurrentTime();
		memcpy(outb.data(), nal[i].p_payload, nal[i].i_payload);
		list << outb;
	}
	newOutputBuffer(0, list);
	return 0;
}
