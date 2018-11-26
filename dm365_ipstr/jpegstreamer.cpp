#include "jpegstreamer.h"
#include "jpegshotserver.h"

#include <lmm/debug.h>
#include <lmm/dm365/dm365camerainput.h>
#include <lmm/bufferqueue.h>
#include <lmm/dmai/jpegencoder.h>
#include <lmm/baselmmpipeline.h>

#include <QUdpSocket>

static bool checkFlag(int flags, int flag)
{
	if (flags & flag)
		return false;
	return true;
}

JpegStreamer::JpegStreamer(int bufferCount, int flags, int qfact, QObject *parent)
	: BaseStreamer(parent)
{
	DM365CameraInput *camIn = new DM365CameraInput;
	camIn->setBufferCount(5);
	camIn->setNonStdOffsets(41, 192);
	camIn->setSize(0, QSize(1920, 1080));
	JpegEncoder *jpeg = new JpegEncoder;
	jpeg->setParameter("videoWidth", 1920);
	jpeg->setParameter("videoHeight", 1080);
	jpeg->setQualityFactor(qfact);
	que1 = new BufferQueue;
	if (!bufferCount)
		bufferCount = 1;
	jpeg->setBufferCount(bufferCount + 10, 1024 * 1024);
	que1->setQueueSize(bufferCount);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(camIn);
	if (checkFlag(flags, 0x04))
		p1->append(jpeg);
	if (checkFlag(flags, 0x08))
		p1->append(que1);
	p1->end();

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(camIn);
	p2->end();

	camIn->getOutputQueue(0)->setRateReduction(25, 15);

	if (checkFlag(flags, 0x01))
		JpegShotServer *server = new JpegShotServer(this, 4571);

	pinger = NULL;
	if (checkFlag(flags, 0x02)) {
		pinger = new QUdpSocket;
		/*
		 * Let's start watchdog ping
		 */
		QByteArray ba;
		ba.append('m');
		ba.append('0');
		ba.append((char)0);
		ba.append((char)0);
		ba.append(1);
		ba.append((char)0);
		ba.append((char)0);
		ba.append((char)0);
		ba.append(1);
		ba.append((char)0);
		ba.append((char)0);
		ba.append((char)0);
		pinger->writeDatagram(ba, QHostAddress::LocalHost, 7878);
		pingmes = ba;
		ba.clear();
		ba.append('m');
		ba.append('1');
		ba.append((char)0);
		ba.append((char)0);
		ba.append((char)1); //capture normally, using for this encoder
		ba.append((char)0);
		ba.append((char)0);
		ba.append((char)0);
		ba.append(1); //1: terminate, 2: kill, 3:reboot
		ba.append((char)0);
		ba.append((char)0);
		ba.append((char)0);
		pinger->writeDatagram(ba, QHostAddress::LocalHost, 7878);
	}
}

QList<RawBuffer> JpegStreamer::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
{
	Q_UNUSED(ch);
	Q_UNUSED(codec);
	return que1->findBuffers(ts, frameCount);
}

int JpegStreamer::pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf)
{
	if (pinger && p == getPipeline(0) && (buf.constPars()->streamBufferNo & 0x8)) {
		/* time-to say goodbye (hello) */
		pinger->writeDatagram(pingmes, QHostAddress::LocalHost, 7878);
	}
	return 0;
}

