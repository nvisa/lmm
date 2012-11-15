#include "videotestsource.h"
#include "streamtime.h"
#include "dmai/dmaibuffer.h"

#include <emdesk/debug.h>

#include <QFile>
#include <QTime>

#include <linux/videodev2.h>

/**
	\class VideoTestSource

	\brief VideoTestSource sinifi, kamera/encoder testlerinde kullanilmak
	icin onceden belirlenmis videolar olusturur.

	VideoTestSource ile onceden belirli videolar olusturabilirsiniz. Su an
	icin sadece statik videolar olusturulabilmektedir. Olusturulacak videolar
	icin onceden belirlenen statik PNG imajlari kullanilir. Calisma esnasinda
	VideoTestSource /etc/lmm/patterns/720p icinde bu PNG imajlarini aramaktadir.
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

VideoTestSource::VideoTestSource(QObject *parent) :
	BaseLmmElement(parent)
{
	width = 1280;
	height = 720;
	lastBufferTime = 0;
	setFps(30);
	pattern = PATTERN_COUNT;
	noisy = false;
}

VideoTestSource::VideoTestSource(int nWidth, int nHeight, QObject *parent)
{
	width = 1280;
	height = 720;
	lastBufferTime = 0;
	setFps(30);
	pattern = PATTERN_COUNT;
	noisy = true;
	noiseWidth = nWidth;
	noiseHeight = nHeight;
}

#define bound(_x) \
	if (_x > 255) _x = 255; \
	if (_x < 0) _x = 0;

void VideoTestSource::setTestPattern(VideoTestSource::TestPattern p)
{
	if (p == pattern)
		return;
	int w, h;
	w = getParameter("videoWidth").toInt();
	h = getParameter("videoHeight").toInt();
	if (w)
		width = w;
	if (h)
		height = h;
	mDebug("creating test pattern %d, widht=%d, height=%d", p, width, height);
	/* we will use cache data if exists */
	QFile f("/tmp/lmmtestpattern.yuv");
	pattern = p;
	pImage = getPatternImage(p);
	bool cvalid = false;

	/* TODO: color space is assumed to be NV12 */
	BufferGfx_Attrs *attr = DmaiBuffer::createGraphicAttrs(width, height, V4L2_PIX_FMT_NV12);
	if (f.exists()) {
		f.open(QIODevice::ReadOnly);
		QByteArray ba = f.readAll();
		if (ba.at(0) == p) {
			for (int i = 0; i < 3; i++) {
				DmaiBuffer imageBuf("video/x-raw-yuv", (ba.constData() + 1)
									  , width * height * 3 / 2, attr, this);
				imageBuf.addBufferParameter("v4l2PixelFormat", V4L2_PIX_FMT_NV12);
				inputBuffers << imageBuf;
			}
			cvalid = true;
		}
		f.close();
	}
	if (!cvalid) {
		DmaiBuffer imageBuf("video/x-raw-yuv", width * height * 3 / 2, attr, this);
		imageBuf.addBufferParameter("v4l2PixelFormat", V4L2_PIX_FMT_NV12);
		inputBuffers << imageBuf;

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
		if (f.open(QIODevice::WriteOnly)) {
			f.write((char *)&pattern, 1);
			f.write((const char *)imageBuf.constData(), imageBuf.size());
		}
	}

	if (noisy && noise.size() == 0) {
		QFile randF("/dev/urandom");
		if (!randF.open(QIODevice::ReadOnly)) {
			mDebug("error opening /dev/urandom");
			noisy = false;
			return;
		}
		for (int i = 0; i < 16; i++) {
			char *nd = new char[noiseHeight * noiseWidth];
			randF.read(nd, noiseHeight * noiseWidth);
			noise << nd;
		}
		randF.close();
	}
	mDebug("test pattern created successfully");
}

void VideoTestSource::setFps(int fps)
{
	targetFps = fps;
	bufferTime = 1000000 / targetFps;
}

RawBuffer VideoTestSource::nextBuffer()
{
	/* TODO: Some clever tolerance decision algorithm is needed */
	int tolerance = 3000;
	inputLock.lock();
	if (!inputBuffers.size()) {
		inputLock.unlock();
		return RawBuffer();
	}
	qint64 time = streamTime->getFreeRunningTime();

	if (time > lastBufferTime + bufferTime - tolerance) {
		lastBufferTime = time;
		/*
		 * creating a new buffer with software copy is so slow,
		 * don't event think about it. But dma may work
		 */
		DmaiBuffer imageBuf = inputBuffers.takeFirst();
		if (noisy)
			imageBuf = addNoise(imageBuf);
		imageBuf.addBufferParameter("captureTime", streamTime->getCurrentTime());
		imageBuf.addBufferParameter("fps", targetFps);
		outputBuffers.append(imageBuf);
		mInfo("time: %lld - %d, buffer count %d", time / 1000, bufferTime, inputBuffers.size());
	}

	inputLock.unlock();
	return BaseLmmElement::nextBuffer();
}

int VideoTestSource::flush()
{
	return 0;
}

QImage VideoTestSource::getPatternImage(VideoTestSource::TestPattern p)
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

DmaiBuffer VideoTestSource::addNoise(DmaiBuffer imageBuf)
{
	/* TODO: color space is assumed to be NV12 */
	char *ydata = (char *)imageBuf.data();
	int offset = imageBuf.size() / 3 * 2 - width * noiseHeight;
	QTime t; t.start();
	static int ni = 0;
	char *ndata = noise.at((ni++ & 0xf));
	for (int i = offset, noff = 0; i < offset + noiseHeight * width; i += width, noff += noiseWidth)
		memcpy(ydata + i + width - noiseWidth, ndata + noff, noiseWidth);
	mInfo("noise addition took %d msecs", t.elapsed());
	return imageBuf;
}
