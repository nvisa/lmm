#include "tx1jpegencoder.h"
#include "debug.h"
#include "nvidia/include/NvBuffer.h"
#include "nvidia/include/NvJpegEncoder.h"

class TX1JpegEncoderPriv {
public:
	NvBuffer *buffer;
	NvJPEGEncoder *enc;
	int quality;
	uchar *outbuf;
	size_t outbufAllocSize;
	size_t outbufUsedSize;
};

static void convertToNv(NvBuffer *buffer, const RawBuffer &rawbuf)
{
	/* convert our rawbuffer into nvbuffer */
	char *src = (char *) rawbuf.constData();
	for (uint i = 0; i < buffer->n_planes; i++)
	{
		NvBuffer::NvBufferPlane &plane = buffer->planes[i];
		std::streamsize bytes_to_read = plane.fmt.bytesperpixel * plane.fmt.width;
		char *data = (char *) plane.data;
		//qDebug() << "plane" << i << plane.fmt.stride << bytes_to_read;
		plane.bytesused = 0;
		for (uint j = 0; j < plane.fmt.height; j++)
		{
			//stream->read(data, bytes_to_read);
			//qDebug() << "will copy" << bytes_to_read << "for plane" << i;
			memcpy(data, src, bytes_to_read);
			//if (stream->gcount() < bytes_to_read)
			//  return -1;
			data += plane.fmt.stride;
			src += bytes_to_read;
		}
		plane.bytesused = plane.fmt.stride * plane.fmt.height;
	}
}

TX1JpegEncoder::TX1JpegEncoder(QObject *parent)
	: BaseLmmElement(parent)
{
	priv = new TX1JpegEncoderPriv;
	priv->buffer = nullptr;
	priv->enc = nullptr;
	priv->quality = 75;
	priv->outbuf = nullptr;
	priv->outbufUsedSize = 0;
}

TX1JpegEncoder::~TX1JpegEncoder()
{
	if (priv->buffer)
		delete priv->buffer;
	if (priv->enc)
		delete priv->enc;
	if (priv->outbuf)
		delete [] priv->outbuf;
	delete priv;
}

void TX1JpegEncoder::setQuality(int q)
{
	priv->quality = q;
}

int TX1JpegEncoder::getQuality()
{
	return priv->quality;
}

int TX1JpegEncoder::processBuffer(const RawBuffer &buf)
{
	if (!priv->buffer) {
		priv->buffer = new NvBuffer(V4L2_PIX_FMT_YUV420M, buf.constPars()->videoWidth,
									   buf.constPars()->videoHeight, 0);
		priv->buffer->allocateMemory();
		priv->enc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
		priv->outbufAllocSize = buf.constPars()->videoWidth * buf.constPars()->videoHeight * 3 / 2;
		priv->outbuf = new uchar[priv->outbufAllocSize];
	}

	/* fill buffer data */
	convertToNv(priv->buffer, buf);
	priv->outbufUsedSize = priv->outbufAllocSize;
	int err = priv->enc->encodeFromBuffer(*priv->buffer, JCS_YCbCr, &priv->outbuf, priv->outbufUsedSize, priv->quality);
	mInfo("New JPEG data with err %d and size %lu", err, priv->outbufUsedSize);
	if (err)
		return 0;

	RawBuffer outbuf("image/jpeg", priv->outbuf, priv->outbufUsedSize);
	outbuf.pars()->videoWidth = buf.constPars()->videoWidth;
	outbuf.pars()->videoWidth = buf.constPars()->videoHeight;
	outbuf.pars()->streamBufferNo = buf.constPars()->streamBufferNo;
	outbuf.pars()->captureTime = buf.constPars()->captureTime;
	return newOutputBuffer(0, outbuf);
}

