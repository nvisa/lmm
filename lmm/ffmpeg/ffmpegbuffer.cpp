#define __STDC_CONSTANT_MACROS
#include "ffmpegbuffer.h"

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
}

FFmpegBuffer::FFmpegBuffer(QString mimeType, AVPacket *packet, BaseLmmElement *parent)
{
	d = new FFmpegBufferData();
	d->myParent = parent;
	d->rawData = NULL;
	d->refData = false;

	d->mimeType = mimeType;

	FFmpegBufferData *dd = (FFmpegBufferData *)d.data();
	dd->packet = packet;
	dd->frame = NULL;
	setRefData(d->mimeType, packet->data, packet->size);
	addBufferParameter("AVPacket", (quintptr)dd->packet);
}

FFmpegBuffer::FFmpegBuffer(QString mimeType, int width, int height, BaseLmmElement *parent)
{
	d = new FFmpegBufferData();
	d->myParent = parent;
	d->rawData = NULL;
	d->refData = false;

	d->mimeType = mimeType;

	FFmpegBufferData *dd = (FFmpegBufferData *)d.data();
	dd->frame = avcodec_alloc_frame();
	dd->packet = NULL;

	// for rgb 24
	dd->frameData = new uchar[width * height * 3];
	avpicture_fill((AVPicture *)dd->frame, dd->frameData, PIX_FMT_RGB24,
				   width, height);
	dd->frameSize = width * height * 3;

	setRefData(d->mimeType, dd->frameData, dd->frameSize);
	addBufferParameter("AVFrame", (quintptr)dd->frame);
}

AVPacket *FFmpegBuffer::getAVPacket()
{
	FFmpegBufferData *dd = (FFmpegBufferData *)d.data();
	return dd->packet;
}

AVFrame *FFmpegBuffer::getAVFrame()
{
	FFmpegBufferData *dd = (FFmpegBufferData *)d.data();
	return dd->frame;
}

static void deletePacket(AVPacket *packet)
{
	if (packet->data != NULL)
		av_free_packet(packet);
	delete packet;
}

static void deleteFrame(AVFrame *frame)
{
	av_free(frame);
}

FFmpegBufferData::~FFmpegBufferData()
{
	if (packet) {
		deletePacket(packet);
		packet = NULL;
	}
	if (frame) {
		deleteFrame(frame);
		frame = NULL;
	}
}
