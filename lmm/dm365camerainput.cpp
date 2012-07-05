#include "dm365camerainput.h"
#include "rawbuffer.h"
#include "streamtime.h"

#include <emdesk/debug.h>

#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>

#include <media/davinci/imp_previewer.h>
#include <media/davinci/imp_resizer.h>
#include <media/davinci/dm365_ipipe.h>

#define RESIZER_DEVICE   "/dev/davinci_resizer"
#define PREVIEWER_DEVICE "/dev/davinci_previewer"

#define V4L2_STD_720P_30        ((v4l2_std_id)(0x0100000000000000ULL))
#define V4L2_STD_720P_60        ((v4l2_std_id)(0x0004000000000000ULL))

#define NUM_CAPTURE_BUFS captureBufferCount

/**
	\class DM365CameraInput

	\brief DM365CameraInput sinifi, adindan da anlasilacagi gibi
	TI DM365 islemcisi uzerinde VPFE kullanarak kamera goruntusu
	yakalanmasina izin verir.

	Bu sinif V4l2Input sinifindan turetilmistir ve bunun uzerinde
	DM365'e ozel yakalama parametrelerini sunar. 2 farkli renk-uzayi
	(colorspace)destekler: V4L2_PIX_FMT_NV12 ve V4L2_PIX_FMT_UYVY.
	Cozunurluk olarak 720p desteklenmektedir.

	setInputType() fonksiyonu ile kamera ayarlarini yapabilirsiniz.
	Burada tip olarak cozunurluk veya renk-uzayindan ziyade onemli
	olan hangi giris tipinin kullanilacagidir. Ornegin giris tipi
	olarak COMPOSITE secerseniz, cozunurluk 480p, renk-uzayi da
	UYVY olarak ayarlanir. Su an icin en cok test edilmis giris
	COMPONENT giristir. (720p ve NV12). Onemli not elimizdeki ham
	cikis veren kameralar(mesela Hitachi) COMPONENT girisi olarak
	kullanilmaktadir.

	Eger renk-uzayi olarak NV12 secilirse, ki kodlayici
	kisitlamalarindan dolayi su an sadece bunu kullanabilirsiniz,
	DM365 preview ve resizer donanim kaynaklari otomatik olarak
	ayarlanarak renk cevrimi yapilir. Unutmayin ki kameralar her zaman
	YUV veri verir. NV12'ye cevrim yazilimsal yada donanimsal olarak
	DM365 tarafindan yapilmak zorundadir.

	DM365CameraInput sinifi programlanabilir sayida (varsayilan 8)
	yakalama tamponu kullanir. Bu tamponlar DMAI kullanarak yaratilir,
	dolayisiyla rahatlikla DMAI tabanli elemanlar tarafindan kullanilabilir.
	Sinirli sayida donanim tamponu oldugundan dolayi, eger ki bufferlar
	kullanilmaz ise hafiza kaybi olusmaz, yani iceride tutulan tampon
	sayisi artmaz, boylece modul kullanilana kadar beklemeye gecmis
	olur.

	\ingroup lmm

	\sa V4l2Input, DM365DmaiCapture
*/

DM365CameraInput::DM365CameraInput(QObject *parent) :
	V4l2Input(parent)
{
	inputType = COMPONENT;
	captureWidth = 1280;
	captureHeight = 720;
	pixFormat = V4L2_PIX_FMT_NV12;
	hCapture = NULL;
	bufTab = NULL;
	captureBufferCount = 8;
}

void DM365CameraInput::aboutDeleteBuffer(const QMap<QString, QVariant> &params)
{
	V4l2Input::aboutDeleteBuffer(params);
}

int DM365CameraInput::openCamera()
{
	struct v4l2_capability cap;
	struct v4l2_input input;
	int width = captureWidth, height = captureHeight;
	int err = 0;

	if (V4L2_PIX_FMT_NV12 == pixFormat) {
		configureResizer();
		configurePreviewer();
	}

	err = openDeviceNode();
	if (err)
		return err;

	input.type = V4L2_INPUT_TYPE_CAMERA;
	input.index = 0;
	err = enumInput(&input);
	while (!err) {
		QString str = QString::fromAscii((const char *)input.name);
		if (inputType == COMPOSITE && str == "Composite")
			break;
		if (inputType == COMPONENT && str == "Component")
			break;
		if (inputType == S_VIDEO && str == "S-Video")
			break;
		if (inputType == SENSOR && str == "Camera")
			break;
		input.index++;
		err = enumInput(&input);
	}
	inputIndex = input.index;
	err = setInput(&input);
	if (err)
		return err;

	v4l2_std_id std_id = V4L2_STD_720P_60;
	err = setStandard(&std_id);
	if (err)
		return err;

	//fpsWorkaround();

	err = queryCapabilities(&cap);
	if (err)
		return err;

	setFormat(pixFormat, width, height);

	/* buffer allocation */
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	gfxAttrs.dim.x = 0;
	gfxAttrs.dim.y = 0;
	gfxAttrs.dim.width = width;
	gfxAttrs.dim.height = height;
	int bufSize;
	if (pixFormat == V4L2_PIX_FMT_UYVY) {
		gfxAttrs.colorSpace = ColorSpace_UYVY;
		gfxAttrs.dim.lineLength = Dmai_roundUp(width * 2, 32);
		bufSize = width * height * 2;
	} else if (pixFormat == V4L2_PIX_FMT_NV12) {
		gfxAttrs.colorSpace = ColorSpace_YUV420PSEMI;
		gfxAttrs.dim.lineLength = Dmai_roundUp(width, 32);
		bufSize = width * height * 3 / 2;
	}
	bufTab = BufTab_create(NUM_CAPTURE_BUFS, bufSize, BufferGfx_getBufferAttrs(&gfxAttrs));
	if (!bufTab) {
		mDebug("unable to create capture buffers");
		return -ENOMEM;
	}
	if (allocBuffers()) {
		mDebug("unable to allocate driver buffers");
		return -ENOMEM;
	}

	return startStreaming();
}

int DM365CameraInput::closeCamera()
{
	stopStreaming();
	close(fd);
	close(rszFd);
	close(preFd);
	qDeleteAll(v4l2buf);
	v4l2buf.clear();
	userptr.clear();
	fd = rszFd = preFd = -1;
	BufTab_delete(bufTab);
	bufsFree.acquire(bufsFree.available());
	mInfo("capture closed");

	return 0;
}

int DM365CameraInput::fpsWorkaround()
{
	struct v4l2_streamparm streamparam;
	Dmai_clear(streamparam);
	streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	streamparam.parm.capture.timeperframe.numerator = 1;
	streamparam.parm.capture.timeperframe.denominator = 30;
	if (ioctl(fd, VIDIOC_S_PARM , &streamparam) < 0) {
		mDebug("VIDIOC_S_PARM failed (%s)", strerror(errno));
		return -errno;
	}
	return 0;
}

Int DM365CameraInput::allocBuffers()
{
	struct v4l2_requestbuffers req;
	struct v4l2_format fmt;

	memset(&fmt, 0, sizeof(v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/*
	 * Tell the driver that we will use user allocated buffers, but don't
	 * allocate any buffers in the driver (just the internal descriptors).
	 */
	Dmai_clear(req);
	memset(&req, 0, sizeof(v4l2_requestbuffers));
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.count  = NUM_CAPTURE_BUFS;
	req.memory = V4L2_MEMORY_USERPTR;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		mDebug("Could not allocate video display buffers");
		return -ENOMEM;
	}

	/* The driver may return less buffers than requested */
	if (req.count < NUM_CAPTURE_BUFS || !req.count) {
		mDebug("Insufficient device driver buffer memory");
		return -ENOMEM;
	}

	/* Allocate space for buffer descriptors */
	/**bufDescsPtr = calloc(numBufs, sizeof(_VideoBufDesc));

	if (*bufDescsPtr == NULL) {
		Dmai_err0("Failed to allocate space for buffer descriptors\n");
		return -ENOMEM;
	}*/

	for (int i = 0; i < (int)req.count; i++) {
		//bufDesc = &(*bufDescsPtr)[bufIdx];
		Buffer_Handle hBuf = BufTab_getBuf(bufTab, i);

		if (hBuf == NULL) {
			mDebug("Failed to get buffer from BufTab for display");
			return -ENOMEM;
		}

		if (Buffer_getType(hBuf) != Buffer_Type_GRAPHICS) {
			mDebug("Buffer supplied to Display not a Graphics buffer");
			return -EINVAL;
		}

		userptr << (char *)Buffer_getUserPtr(hBuf);
		v4l2buf << new struct v4l2_buffer;
		memset(v4l2buf[i], 0, sizeof(v4l2_buffer));
		v4l2buf[i]->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		v4l2buf[i]->memory = V4L2_MEMORY_USERPTR;
		v4l2buf[i]->index = i;
		v4l2buf[i]->m.userptr = (int)Buffer_getUserPtr(hBuf);
		v4l2buf[i]->length = Buffer_getSize(hBuf);

		//		bufDesc->hBuf              = hBuf;
		//		bufDesc->used              = (queueBuffers == FALSE) ? TRUE : FALSE;

		/* If queueBuffers is TRUE, initialize the buffers to black and queue
		 * them into the driver.
		 */

		Buffer_setNumBytesUsed(hBuf, Buffer_getSize(hBuf));

		/* Queue buffer in device driver */
		if (ioctl(fd, VIDIOC_QBUF, v4l2buf[i]) == -1) {
			mDebug("VIODIC_QBUF failed (%s)", strerror(errno));
			return -errno;
		}
	}
	bufsFree.release(NUM_CAPTURE_BUFS - 1);

	return 0;
}

int DM365CameraInput::putFrame(v4l2_buffer *buffer)
{
	mInfo("giving back frame %p to driver", buffer);
	buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* Issue captured frame buffer back to device driver */
	if (ioctl(fd, VIDIOC_QBUF, buffer) == -1) {
		mDebug("VIDIOC_QBUF failed (%s)", strerror(errno));
		return -errno;
	}

	return 0;
}

v4l2_buffer * DM365CameraInput::getFrame()
{
	struct v4l2_buffer buffer;
	memset(&buffer, 0, sizeof(v4l2_buffer));
	buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory = V4L2_MEMORY_USERPTR;

	/* Get a frame buffer with captured data */
	if (ioctl(fd, VIDIOC_DQBUF, &buffer) < 0) {
		mDebug("VIDIOC_DQBUF failed with err %d", -errno);
		return NULL;
	}

	return v4l2buf[buffer.index];
}

bool DM365CameraInput::captureLoop()
{
	finishedLock.lock();
	while (finishedBuffers.size()) {
		putFrame(finishedBuffers.takeFirst());
		bufsFree.release(1);
	}
	finishedLock.unlock();
	if (!bufsFree.tryAcquire(1, 20)) {
		mDebug("no kernel buffers available");
		return false;
	}

	struct v4l2_buffer *buffer = getFrame();
	finishedLock.lock();
	while (finishedBuffers.size()) {
		putFrame(finishedBuffers.takeFirst());
		bufsFree.release(1);
	}
	finishedLock.unlock();
	if (buffer) {
		//bufsTaken.release(1);
		int passed = timing.restart();
		mInfo("captured %p, time is %lld, passed %d", buffer, streamTime->getFreeRunningTime(), passed);
		Buffer_Handle dmaibuf = BufTab_getBuf(bufTab, buffer->index);
		char *data = userptr[buffer->index];
		RawBuffer newbuf = RawBuffer();
		newbuf.setParentElement(this);

		newbuf.setRefData(data, buffer->length);
		newbuf.addBufferParameter("width", (int)captureWidth);
		newbuf.addBufferParameter("height", (int)captureHeight);
		newbuf.addBufferParameter("v4l2Buffer",
								  qVariantFromValue((void *)buffer));
		newbuf.addBufferParameter("dmaiBuffer", (int)dmaibuf);
		newbuf.addBufferParameter("dataPtr", (int)data);
		newbuf.addBufferParameter("creationTime", streamTime->getCurrentTime());
		newbuf.addBufferParameter("captureTime", streamTime->getCurrentTime());
		outputLock.lock();
		outputBuffers << newbuf;
		outputLock.unlock();

		if (passed > 35)
			mInfo("late capture: %d", passed);
	}

	return false;
}

int DM365CameraInput::configureResizer(void)
{
	unsigned int oper_mode, user_mode;
	struct rsz_channel_config rsz_chan_config;
	struct rsz_continuous_config rsz_cont_config;

	user_mode = IMP_MODE_CONTINUOUS;
	rszFd = open((const char *)RESIZER_DEVICE, O_RDWR);
	if(rszFd <= 0) {
		mDebug("Cannot open resizer device");
		return NULL;
	}

	if (ioctl(rszFd, RSZ_S_OPER_MODE, &user_mode) < 0) {
		mDebug("Can't set operation mode (%s)", strerror(errno));
		close(rszFd);
		return -errno;
	}

	if (ioctl(rszFd, RSZ_G_OPER_MODE, &oper_mode) < 0) {
		mDebug("Can't get operation mode (%s)", strerror(errno));
		close(rszFd);
		return -errno;
	}

	if (oper_mode == user_mode) {
		mInfo("Successfully set mode to continuous in resizer");
	} else {
		mDebug("Failed to set mode to continuous in resizer\n");
		close(rszFd);
		return -EINVAL;
	}

	/* set configuration to chain resizer with preview */
	rsz_chan_config.oper_mode = user_mode;
	rsz_chan_config.chain  = 1;
	rsz_chan_config.len = 0;
	rsz_chan_config.config = NULL; /* to set defaults in driver */
	if (ioctl(rszFd, RSZ_S_CONFIG, &rsz_chan_config) < 0) {
		mDebug("Error in setting default configuration in resizer (%s)",
			   strerror(errno));
		close(rszFd);
		return -errno;
	}

	bzero(&rsz_cont_config, sizeof(struct rsz_continuous_config));
	rsz_chan_config.oper_mode = user_mode;
	rsz_chan_config.chain = 1;
	rsz_chan_config.len = sizeof(struct rsz_continuous_config);
	rsz_chan_config.config = &rsz_cont_config;

	if (ioctl(rszFd, RSZ_G_CONFIG, &rsz_chan_config) < 0) {
		mDebug("Error in getting channel configuration from resizer (%s)\n",
			   strerror(errno));
		close(rszFd);
		return -errno;
	}

	/* we can ignore the input spec since we are chaining. So only
	   set output specs */
	rsz_cont_config.output1.enable = 1;
	rsz_cont_config.output2.enable = 0;
	rsz_chan_config.len = sizeof(struct rsz_continuous_config);
	rsz_chan_config.config = &rsz_cont_config;
	if (ioctl(rszFd, RSZ_S_CONFIG, &rsz_chan_config) < 0) {
		mDebug("Error in setting resizer configuration (%s)",
			   strerror(errno));
		close(rszFd);
		return -errno;
	}
	mInfo("Resizer initialized");
	return 0;
}

int DM365CameraInput::configurePreviewer()
{
	unsigned int oper_mode, user_mode;
	struct prev_channel_config prev_chan_config;
	user_mode = IMP_MODE_CONTINUOUS;

	preFd = open((const char *)PREVIEWER_DEVICE, O_RDWR);
	if(preFd <= 0) {
		mDebug("Cannot open previewer device \n");
		return -errno;
	}

	if (ioctl(preFd, PREV_S_OPER_MODE, &user_mode) < 0) {
		mDebug("Can't set operation mode in previewer (%s)", strerror(errno));
		close(preFd);
		return -errno;
	}

	if (ioctl(preFd, PREV_G_OPER_MODE, &oper_mode) < 0) {
		mDebug("Can't get operation mode from previewer (%s)", strerror(errno));
		close(preFd);
		return -errno;
	}

	if (oper_mode == user_mode) {
		mInfo("Operating mode changed successfully to continuous in previewer");
	} else {
		mDebug("failed to set mode to continuous in previewer");
		close(preFd);
		return -errno;
	}

	prev_chan_config.oper_mode = oper_mode;
	prev_chan_config.len = 0;
	prev_chan_config.config = NULL; /* to set defaults in driver */

	if (ioctl(preFd, PREV_S_CONFIG, &prev_chan_config) < 0) {
		mDebug("Error in setting default previewer configuration (%s)",
			   strerror(errno));
		close(preFd);
		return -errno;
	}

	mInfo("Previewer initialized");
	return preFd;
}
