#include "ffmpegbuffer.h"

extern "C" {
	#define __STDC_CONSTANT_MACROS
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
	setRefData(d->mimeType, packet->data, packet->size);
	addBufferParameter("AVPacket", (quintptr)dd->packet);
}

AVPacket *FFmpegBuffer::getAVPacket()
{
	FFmpegBufferData *dd = (FFmpegBufferData *)d.data();
	return dd->packet;
}

static void deletePacket(AVPacket *packet)
{
	if (packet->data != NULL)
		av_free_packet(packet);
	delete packet;
}

FFmpegBufferData::~FFmpegBufferData()
{
	if (packet) {
		deletePacket(packet);
		packet = NULL;
	}
}
