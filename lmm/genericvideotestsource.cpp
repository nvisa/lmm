#include "genericvideotestsource.h"
#include "streamtime.h"
#include "lmmthread.h"
#include "lmmbufferpool.h"

#include "debug.h"

#include <QFile>
#include <QSemaphore>
#include <QElapsedTimer>

#include <errno.h>
#include <unistd.h>
#include <linux/videodev2.h>

/**
	\class GenericVideoTestSource

	\brief GenericGenericVideoTestSource sinifi, kamera/encoder testlerinde kullanilmak
	icin onceden belirlenmis videolar olusturur.

	GenericVideoTestSource ile onceden belirli videolar olusturabilirsiniz. Su an
	icin sadece statik videolar olusturulabilmektedir. Olusturulacak videolar
	icin onceden belirlenen statik PNG imajlari kullanilir. Calisma esnasinda
	GenericVideoTestSource /etc/lmm/patterns/720p icinde bu PNG imajlarini aramaktadir.
	Eger burada PNG imajlari bulunamazsa karsiniza test esnasinda pembe bir ekran
	cikacaktir. Su an icin sadece 720p imajlar desteklenmedir, ileride 480p, 1080p
	imajlarda desteklenebilir.

	Bu sinif okudugu PNG icindeki RGB resmi uygun bir YUV formatina cevirir. Su an
	icin sadece NV12(ayrik luma ve kroma duzlemleri, girisik(interleaved) kroma
	duzlemi, 4:2:0 kroma altorneklemesi) desteklenmektedir. Cevirme islemi yavas
	oldugu icin cevrilen buffer /tmp/lmmtestpattern.yuv altinda saklanir ve
	bir sonraki calma islemlerinde desen degismedigi surece kullanilir. Dolayisi ile
	cihaz acildiginda ilk calma islemi cok yavas olabilir, diger seferlerde hizlanacaktir.

	Bu siniftan bir ornek yarattiktan sonra setTestPattern() fonksiyonu ile
	olusturmak istediginiz deseni secmeniz yeterli olacaktir.

	Varsayilan olarak 25 fps kullanilmaktadir, ama setFps() fonksiyonu ile istediginiz
	degeri gecebilirsiniz.

	\ingroup lmm

	\sa DM365CameraInput
*/

#define NUM_OF_BUFFERS 3

GenericVideoTestSource::GenericVideoTestSource(QObject *parent) :
	BaseLmmElement(parent)
{
	width = 1280;
	height = 720;
	lastBufferTime = 0;
	setFps(30);
	pattern = PATTERN_COUNT;
	noisy = false;
	pool = new LmmBufferPool;
}

GenericVideoTestSource::GenericVideoTestSource(int nWidth, int nHeight, QObject *parent) :
	BaseLmmElement(parent)
{
	width = 1280;
	height = 720;
	lastBufferTime = 0;
	setFps(30);
	pattern = PATTERN_COUNT;
	noisy = true;
	noiseWidth = nWidth;
	noiseHeight = nHeight;
	pool = new LmmBufferPool;
}

#define bound(_x) \
	if (_x > 255) _x = 255; \
	if (_x < 0) _x = 0;

int GenericVideoTestSource::setTestPattern(GenericVideoTestSource::TestPattern p)
{
	if (p == pattern)
		return 0;
	int w, h;
	w = getParameter("videoWidth").toInt();
	h = getParameter("videoHeight").toInt();
	if (w)
		width = w;
	if (h)
		height = h;
	mDebug("creating test pattern %d, width=%d, height=%d", p, width, height);
	pattern = p;
	pImage = getPatternImage(p);

	/* TODO: color space is assumed to be NV12 */

	if (p == RAW_YUV_FILE || p == RAW_YUV_VIDEO) {
		/* cache is not used in this mode */
		return 0;
	}
	/* we will use cache data if exists */
	if (!checkCache(p)) {
		mDebug("cache is not valid");
		RawBuffer imageBuf("video/x-raw-yuv", width * height * 3 / 2);
		imageBuf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
		addBufferToPool(imageBuf);

		uchar *ydata = (uchar *)imageBuf.constData();
		uchar *cdata = ydata + imageBuf.size() / 3 * 2;

		/* scale image if necessary */
		if (pImage.width() != width || pImage.height() != height)
			pImage = pImage.scaled(width, height);

		int cindex = 0;
		/* do colorspace conversion */
		for (int j = 0; j < pImage.height(); j++) {
			for (int i = 0; i < pImage.width(); i++) {
				int R = qRed(pImage.pixel(i, j));
				int G = qGreen(pImage.pixel(i, j));
				int B = qBlue(pImage.pixel(i, j));
				int Y = (0.257 * R) + (0.504 * G) + (0.098 * B) + 16;
				int V = (0.439 * R) - (0.368 * G) - (0.071 * B) + 128;
				int U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128;
				bound(Y);
				bound(U);
				bound(V);

				/* 4:2:0 chroma subsampling */
				ydata[j * pImage.width() + i] = Y;
				if ((i & 0x1) == 0 && (j & 0x1) == 0) {
					cdata[cindex] = U;
					cdata[cindex + 1] = V;
					cindex += 2;
				}
			}
		}

		/* write cache data so that next time we will be faster */
		QFile f("/tmp/lmmtestpattern.yuv");
		if (f.open(QIODevice::WriteOnly)) {
			f.write((char *)&pattern, 1);
			f.write((const char *)imageBuf.constData(), imageBuf.size());
		}

		for (int i = 1; i < NUM_OF_BUFFERS; i++) {
			RawBuffer imageBuf2("video/x-raw-yuv", imageBuf.constData()
								  , width * height * 3 / 2);
			imageBuf2.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
			addBufferToPool(imageBuf2);
		}
	}

	if (noisy && noise.size() == 0) {
		mDebug("creating noise");
		QFile randF("/dev/urandom");
		if (!randF.open(QIODevice::ReadOnly)) {
			mDebug("error opening /dev/urandom");
			noisy = false;
			return -ENODEV;
		}
		for (int i = 0; i < 16; i++) {
			char *nd = new char[noiseHeight * noiseWidth];
			randF.read(nd, noiseHeight * noiseWidth);
			noise << nd;
		}
		randF.close();
	}
	mDebug("test pattern created successfully");
	return 0;
}

void GenericVideoTestSource::setFps(int fps)
{
	targetFps = fps;
	bufferTime = 1000000 / targetFps;
}

void GenericVideoTestSource::setYUVFile(QString filename)
{
	QFile f(filename);
	if (f.exists()) {
		f.open(QIODevice::ReadOnly);
		QByteArray ba = f.read(width * height * 3 / 2);
		/* TODO: color space is assumed to be NV12 */
		for (int i = 0; i < NUM_OF_BUFFERS; i++) {
			RawBuffer imageBuf("video/x-raw-yuv", (ba.constData())
								, width * height * 3 / 2);
			imageBuf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
			addBufferToPool(imageBuf);
		}

		f.close();
	}
	pattern = RAW_YUV_FILE;
}

void GenericVideoTestSource::setYUVVideo(QString filename, bool loop)
{
	loopVideoFile = loop;
	pattern  = RAW_YUV_VIDEO;
	if (videoFile.isOpen())
		videoFile.close();
	videoFile.setFileName(filename);
	videoFile.open(QIODevice::ReadOnly);

	/* TODO: color space is assumed to be NV12 */
	for (int i = 0; i < NUM_OF_BUFFERS; i++) {
		RawBuffer imageBuf("video/x-raw-yuv", width * height * 3 / 2);
		imageBuf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
		addBufferToPool(imageBuf);
	}
}

int GenericVideoTestSource::processBlocking(int ch)
{
	if (getState() == STOPPED)
		return -EINVAL;
	//QElapsedTimer t; t.start();
	//while (t.elapsed() * 1000 < bufferTime)
		usleep(bufferTime);
	RawBuffer buf("", ch);
	notifyEvent(EV_PROCESS, buf);
	int err = processBuffer(buf);
	return err;
}

int GenericVideoTestSource::processBuffer(const RawBuffer &buf)
{
	mInfo("creating next frame");
	RawBuffer imageBuf = pool->take(false);
	if (pattern == RAW_YUV_VIDEO) {
		if (videoFile.isOpen() == false)
			return -EINVAL;
		videoFile.read((char *)imageBuf.data(), width * height * 3 / 2);
		if (videoFile.atEnd()) {
			videoFile.close();
			if (loopVideoFile)
				videoFile.open(QIODevice::ReadOnly);
		}
	}
	if (noisy)
		imageBuf = addNoise(imageBuf);
	imageBuf.pars()->captureTime = streamTime->getCurrentTime();
	imageBuf.pars()->fps = targetFps;
	int ch = buf.size();
	return newOutputBuffer(ch, imageBuf);
}

void GenericVideoTestSource::addBufferToPool(RawBuffer buf)
{
	buf.pars()->poolIndex = refBuffers.size();
	refBuffers.insert(refBuffers.size(), buf);
	RawBuffer buf2(this);
	buf2.setRefData(buf.getMimeType(), buf.data(), buf.size());
	buf2.pars()->poolIndex = buf.pars()->poolIndex;
	pool->addBuffer(buf2);
}

void GenericVideoTestSource::aboutToDeleteBuffer(const RawBufferParameters *params)
{
	RawBuffer &buf = refBuffers[params->poolIndex];
	RawBuffer buf2(this);
	buf2.setRefData(buf.getMimeType(), buf.data(), buf.size());
	buf2.pars()->poolIndex = buf.pars()->poolIndex;
	pool->give(buf2);
}

int GenericVideoTestSource::flush()
{
	return 0;
}

QImage GenericVideoTestSource::getPatternImage(GenericVideoTestSource::TestPattern p)
{
	QString str = "/etc/lmm/patterns/720p/%1";
	if (p == COLORBARS)
		return QImage(str.arg("colorbars.png"));
	if (p == COLORCHART)
		return QImage(str.arg("color-chart.png"));
	if (p == COLORSQUARES)
		return QImage(str.arg("colorsquares.png"));
	if (p == GRADIENT_16)
		return QImage(str.arg("Gradient-16bit.png"));
	if (p == HEX_9X5)
		return QImage(str.arg("hex-9x5.png"));
	if (p == LINEAR_ZONEPLATE)
		return QImage(str.arg("Linear-ZonePlate.png"));
	if (p == SQUARE_WEDGES)
		return QImage(str.arg("Square-wedges-80grey.png"));
	if (p == STAR_BARS_144)
		return QImage(str.arg("star-chart-bars144-600dpi.png"));
	if (p == STAR_BARS_FULL)
		return QImage(str.arg("star-chart-bars-full-600dpi.png"));
	if (p == STAR_SINE_144)
		return QImage(str.arg("star-chart-sine144-720dpi.png"));
	if (p == TRT)
		return QImage(str.arg("trt.png"));
	if (p == ZONE_HARDEDGE)
		return QImage(str.arg("Zone720-hardedge-A.png"));

	mDebug("unknown pattern type, returning default one");
	return QImage(str.arg("colorbars.png"));
}

RawBuffer GenericVideoTestSource::addNoise(RawBuffer imageBuf)
{
	/* TODO: color space is assumed to be NV12 */
	char *ydata = (char *)imageBuf.data();
	int offset = imageBuf.size() / 3 * 2 - width * noiseHeight;
	QElapsedTimer t; t.start();
	static int ni = 0;
	char *ndata = noise.at((ni++ & 0xf));
	for (int i = offset, noff = 0; i < offset + noiseHeight * width; i += width, noff += noiseWidth)
		memcpy(ydata + i + width - noiseWidth, ndata + noff, noiseWidth);
	mInfo("noise addition took %lld msecs", t.elapsed());
	return imageBuf;
}

bool GenericVideoTestSource::checkCache(TestPattern p)
{
	bool valid = false;
	QFile f("/tmp/lmmtestpattern.yuv");
	if (f.exists()) {
		f.open(QIODevice::ReadOnly);
		QByteArray ba = f.readAll();
		if (ba.at(0) == p) {
			for (int i = 0; i < NUM_OF_BUFFERS; i++) {
				RawBuffer imageBuf("video/x-raw-yuv", (ba.constData() + 1)
									  , width * height * 3 / 2);
				imageBuf.pars()->v4l2PixelFormat =V4L2_PIX_FMT_NV12;
				addBufferToPool(imageBuf);
			}
			valid = true;
		}
		f.close();
	}
	return valid;
}
