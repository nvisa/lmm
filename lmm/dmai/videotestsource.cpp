#include "videotestsource.h"
#include "streamtime.h"
#include "dmai/dmaibuffer.h"
#include "lmmthread.h"
#include "lmmbufferpool.h"

#include "debug.h"

#include <QFile>
#include <QSemaphore>
#include <QElapsedTimer>

#include <errno.h>
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

#define NUM_OF_BUFFERS 3

VideoTestSource::VideoTestSource(QObject *parent) :
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

VideoTestSource::VideoTestSource(int nWidth, int nHeight, QObject *parent) :
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

int VideoTestSource::setTestPattern(VideoTestSource::TestPattern p)
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

	refBuffers.clear();

	if (p == RAW_YUV_FILE || p == RAW_YUV_VIDEO) {
		/* cache is not used in this mode */
		return 0;
	}

	/* TODO: color space is assumed to be NV12 */
	BufferGfx_Attrs *attr = DmaiBuffer::createGraphicAttrs(width, height, V4L2_PIX_FMT_NV12);
	pImage = getPatternImage(p);

	/* we will use cache data if exists */
	if (!checkCache(p, attr)) {
		mDebug("cache is not valid");
		DmaiBuffer imageBuf("video/x-raw-yuv", width * height * 3 / 2, attr);
		imageBuf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
		refBuffers.insert((int)imageBuf.getDmaiBuffer(), imageBuf);
		DmaiBuffer tmp("video/x-raw-yuv", imageBuf.getDmaiBuffer(), this);
		tmp.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
		addBufferToPool(tmp);

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
			DmaiBuffer imageBuf2("video/x-raw-yuv", imageBuf.constData()
								  , width * height * 3 / 2, attr);
			imageBuf2.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
			refBuffers.insert((int)imageBuf2.getDmaiBuffer(), imageBuf2);
			DmaiBuffer tmp2("video/x-raw-yuv", imageBuf2.getDmaiBuffer(), this);
			tmp2.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
			addBufferToPool(tmp);
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

void VideoTestSource::setFps(int fps)
{
	targetFps = fps;
	bufferTime = 1000000 / targetFps;
}

void VideoTestSource::setYUVFile(QString filename)
{
	QFile f(filename);
	if (f.exists()) {
		setTestPattern(RAW_YUV_FILE);
		f.open(QIODevice::ReadOnly);
		QByteArray ba = f.read(width * height * 3 / 2);
		/* TODO: color space is assumed to be NV12 */
		BufferGfx_Attrs *attr = DmaiBuffer::createGraphicAttrs(width, height, V4L2_PIX_FMT_NV12);
		for (int i = 0; i < NUM_OF_BUFFERS; i++) {
			DmaiBuffer imageBuf("video/x-raw-yuv", (ba.constData())
								, width * height * 3 / 2, attr);
			imageBuf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
			refBuffers.insert((int)imageBuf.getDmaiBuffer(), imageBuf);
			DmaiBuffer tmp("video/x-raw-yuv", imageBuf.getDmaiBuffer(), this);
			tmp.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
			addBufferToPool(tmp);
		}

		f.close();
	}
	//pattern = RAW_YUV_FILE;
}

void VideoTestSource::setYUVVideo(QString filename, bool loop)
{
	loopVideoFile = loop;
	pattern  = RAW_YUV_VIDEO;
	if (videoFile.isOpen())
		videoFile.close();
	videoFile.setFileName(filename);
	videoFile.open(QIODevice::ReadOnly);

	/* TODO: color space is assumed to be NV12 */
	BufferGfx_Attrs *attr = DmaiBuffer::createGraphicAttrs(width, height, V4L2_PIX_FMT_NV12);
	for (int i = 0; i < NUM_OF_BUFFERS; i++) {
		DmaiBuffer imageBuf("video/x-raw-yuv", width * height * 3 / 2, attr);
		imageBuf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
		refBuffers.insert((int)imageBuf.getDmaiBuffer(), imageBuf);
		DmaiBuffer tmp("video/x-raw-yuv", imageBuf.getDmaiBuffer(), this);
		tmp.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
		addBufferToPool(tmp);
	}
}

int VideoTestSource::processBlocking(int ch)
{
	if (getState() == STOPPED)
		return -EINVAL;
	QElapsedTimer t; t.start();
	while (t.elapsed() * 1000 < bufferTime)
		usleep(bufferTime);
	RawBuffer buf("", ch);
	notifyEvent(EV_PROCESS, buf);
	int err = processBuffer(buf);
	//ffDebug() << t.elapsed() << streamTime->getCurrentTime();
	return err;
}

int VideoTestSource::processBuffer(const RawBuffer &buf)
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

void VideoTestSource::addBufferToPool(RawBuffer buf)
{
	pool->addBuffer(buf);
}

void VideoTestSource::aboutToDeleteBuffer(const RawBufferParameters *params)
{
	DmaiBuffer tmp("video/x-raw-yuv", (Buffer_Handle)params->dmaiBuffer, this);
	tmp.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
	pool->give(tmp);
}

int VideoTestSource::flush()
{
	return 0;
}

int VideoTestSource::start()
{
	return BaseLmmElement::start();
}

int VideoTestSource::stop()
{
	//pool->finalize();
	return BaseLmmElement::stop();
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
	QElapsedTimer t; t.start();
	static int ni = 0;
	char *ndata = noise.at((ni++ & 0xf));
	for (int i = offset, noff = 0; i < offset + noiseHeight * width; i += width, noff += noiseWidth)
		memcpy(ydata + i + width - noiseWidth, ndata + noff, noiseWidth);
	mInfo("noise addition took %lld msecs", t.elapsed());
	return imageBuf;
}

bool VideoTestSource::checkCache(TestPattern p, BufferGfx_Attrs *attr)
{
	bool valid = false;
	QFile f("/tmp/lmmtestpattern.yuv");
	if (f.exists()) {
		f.open(QIODevice::ReadOnly);
		QByteArray ba = f.readAll();
		if (ba.at(0) == p) {
			for (int i = 0; i < NUM_OF_BUFFERS; i++) {
				DmaiBuffer imageBuf("video/x-raw-yuv", (ba.constData() + 1)
									  , width * height * 3 / 2, attr);
				imageBuf.pars()->v4l2PixelFormat =V4L2_PIX_FMT_NV12;
				refBuffers.insert((int)imageBuf.getDmaiBuffer(), imageBuf);
				DmaiBuffer tmp("video/x-raw-yuv", imageBuf.getDmaiBuffer(), this);
				tmp.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
				addBufferToPool(tmp);
			}
			valid = true;
		}
		f.close();
	}
	return valid;
}
