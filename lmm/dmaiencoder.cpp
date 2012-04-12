#include "dmaiencoder.h"
#include "rawbuffer.h"
#include "emdesk/debug.h"

#include <ti/sdo/ce/CERuntime.h>

#include <errno.h>

DmaiEncoder::DmaiEncoder(QObject *parent) :
	BaseLmmElement(parent)
{
	imageWidth = 1280;
	imageHeight = 720;
	codec = CODEC_H264;
}

void DmaiEncoder::setImageSize(QSize s)
{
	imageWidth = s.width();
	imageHeight = s.height();
}

int DmaiEncoder::setCodecType(DmaiEncoder::CodecType type)
{
	if (type == CODEC_MPEG2)
		return -EINVAL;
	codec = type;
	return 0;
}

int DmaiEncoder::start()
{
	encodeCount = 0;
	int err = startCodec();
	if (err)
		return err;
	return BaseLmmElement::start();
}

int DmaiEncoder::stop()
{
	int err = stopCodec();
	if (err)
		return err;
	if (hEngine) {
		mDebug("closing codec engine");
		Engine_close(hEngine);
		hEngine = NULL;
	}
	return BaseLmmElement::stop();
}

int DmaiEncoder::flush()
{
	return BaseLmmElement::flush();
}

int DmaiEncoder::encodeNext()
{
	int err = 0;
	inputLock.lock();
	if (inputBuffers.size() == 0) {
		inputLock.unlock();
		return -ENOENT;
	}
	RawBuffer buf = inputBuffers.takeFirst();
	inputLock.unlock();
	Buffer_Handle dmai = (Buffer_Handle)buf.getBufferParameter("dmaiBuffer")
			.toInt();
	if (!dmai) {
		mDebug("cannot get dmai buffer");
		err = -ENOENT;
		goto out;
	}
	err = encode(dmai);
	if (err)
		goto out;
out:
	return err;
}

void DmaiEncoder::aboutDeleteBuffer(const QMap<QString, QVariant> &params)
{
	Buffer_Handle dmai = (Buffer_Handle)params["dmaiBuffer"].toInt();
	bufferLock.lock();
	BufTab_freeBuf(dmai);
	bufferLock.unlock();
	mInfo("freeing buffer");
}

void DmaiEncoder::initCodecEngine()
{
	CERuntime_init();
	Dmai_init();
}
