#include "dm365dmaicapture.h"
#include "dmai/dmaibuffer.h"

#include <emdesk/debug.h>

#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <QThread>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Capture.h>

#include <media/davinci/imp_previewer.h>
#include <media/davinci/imp_resizer.h>
#include <media/davinci/dm365_ipipe.h>

#define RESIZER_DEVICE   "/dev/davinci_resizer"
#define PREVIEWER_DEVICE "/dev/davinci_previewer"

#define V4L2_STD_720P_30        ((v4l2_std_id)(0x0100000000000000ULL))
#define V4L2_STD_720P_60        ((v4l2_std_id)(0x0004000000000000ULL))

#define NUM_CAPTURE_BUFS 5

DM365DmaiCapture::DM365DmaiCapture(QObject *parent) :
	V4l2Input(parent)
{
	inputType = COMPONENT;
	captureWidth = 1280;
	captureHeight = 720;
	pixFormat = V4L2_PIX_FMT_UYVY;
	outPixFormat = V4L2_PIX_FMT_NV12;
	hCapture = NULL;
	bufTab = NULL;
}

QSize DM365DmaiCapture::captureSize()
{
	return QSize(captureWidth, captureHeight);
}

int DM365DmaiCapture::putFrameDmai(Buffer_Handle handle)
{
	mInfo("putting back %p", handle);
	if (Capture_put(hCapture, handle) < 0) {
		mDebug("Failed to put capture buffer");
		return -EINVAL;
	}

	return 0;
}

Buffer_Handle DM365DmaiCapture::getFrameDmai()
{
	Buffer_Handle handle;
	if (Capture_get(hCapture, &handle) < 0) {
		mDebug("Failed to get capture buffer");
		return NULL;
	}
	return handle;
}

bool DM365DmaiCapture::captureLoop()
{
	while (inputBuffers.size()) {
		RawBuffer buffer = inputBuffers.takeFirst();
		Buffer_Handle dmai = (Buffer_Handle)buffer.
							 getBufferParameter("dmaiBuffer").toInt();
		putFrameDmai(dmai);
		bufsFree.release(1);
		if (!bufferPool.contains(dmai))
			bufferPool.insert(dmai, buffer);
	}
	if (!bufsFree.tryAcquire(1, 1000)) {
		mDebug("no kernel buffers available");
		return false;
	}
	Buffer_Handle dmaibuf = getFrameDmai();
	if (!dmaibuf) {
		mDebug("empty buffer");
		return NULL;
	}
	outputBuffers << bufferPool[dmaibuf];
	mInfo("captured %p, time is %d", dmaibuf, timing.elapsed());

	return false;
}

void DM365DmaiCapture::aboutDeleteBuffer(const QMap<QString, QVariant> &params)
{
	Buffer_Handle dmai = (Buffer_Handle)params["dmaiBuffer"].toInt();
	RawBuffer buffer = createNewRawBuffer(dmai);
	inputBuffers << buffer;
	mDebug("buffer finished");
}

/**
 *
 * When DM365 ipipe works in cont mode(normally)
 * then imp_chained is 1 and color space cannot
 * be NV12(YUV420PSEMI)
 */
int DM365DmaiCapture::openCamera()
{
	Capture_Attrs cAttrs   = Capture_Attrs_DM365_DEFAULT;
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	BufferGfx_Dimensions  capDim;
	VideoStd_Type         videoStd;
	Int32                 bufSize;
	ColorSpace_Type       colorSpaceBuffers = ColorSpace_YUV420PSEMI;
	ColorSpace_Type       colorSpaceCap = ColorSpace_YUV420PSEMI;
	Int                   numCapBufs;

	if (outPixFormat != pixFormat) {
		configureResizer();
		configurePreviewer();
	}

	/* When we use 720P_30 we get error */
	videoStd = VideoStd_720P_60;

	/* Calculate the dimensions of a video standard given a color space */
	if (BufferGfx_calcDimensions(videoStd, colorSpaceBuffers, &capDim) < 0) {
		mDebug("Failed to calculate Buffer dimensions");
		return -EINVAL;
	}

	/*
	 * Capture driver provides 32 byte aligned data. We 32 byte align the
	 * capture and video buffers to perform zero copy encoding.
	 */
	capDim.width = Dmai_roundUp(capDim.width, 32);
	captureWidth = capDim.width;
	captureHeight = capDim.height;

	numCapBufs = NUM_CAPTURE_BUFS;

	gfxAttrs.dim.height = capDim.height;
	gfxAttrs.dim.width = capDim.width;
	gfxAttrs.dim.lineLength =
			Dmai_roundUp(BufferGfx_calcLineLength(gfxAttrs.dim.width,
												  colorSpaceBuffers), 32);
	gfxAttrs.dim.x = 0;
	gfxAttrs.dim.y = 0;
	if (colorSpaceBuffers ==  ColorSpace_YUV420PSEMI) {
		bufSize = gfxAttrs.dim.lineLength * gfxAttrs.dim.height * 3 / 2;
	}
	else {
		bufSize = gfxAttrs.dim.lineLength * gfxAttrs.dim.height * 2;
	}

	/* Create a table of buffers to use with the capture driver */
	gfxAttrs.colorSpace = colorSpaceBuffers;
	bufTab = BufTab_create(numCapBufs, bufSize,
							BufferGfx_getBufferAttrs(&gfxAttrs));
	if (bufTab == NULL) {
		mDebug("Failed to create buftab");
		return -ENOMEM;
	}

	/* Create capture device driver instance */
	cAttrs.numBufs = NUM_CAPTURE_BUFS;
	cAttrs.videoInput = Capture_Input_COMPONENT;
	if (inputType == SENSOR)
		cAttrs.videoInput = Capture_Input_CAMERA;
	cAttrs.videoStd = videoStd;
	cAttrs.colorSpace = colorSpaceCap;
	cAttrs.captureDimension = &gfxAttrs.dim;
	cAttrs.onTheFly = false;

	/* Create the capture device driver instance */
	hCapture = Capture_create(bufTab, &cAttrs);

	if (hCapture == NULL) {
		mDebug("Failed to create capture device. Is video input connected?");
		return -ENOENT;
	}
	fd = *((int *)hCapture);
	//fpsWorkaround();

	for (int i = 0; i < BufTab_getNumBufs(bufTab); i++) {
		Buffer_Handle hBuf = BufTab_getBuf(bufTab, i);
		createNewRawBuffer(hBuf);
	}
	bufsFree.release(NUM_CAPTURE_BUFS - 1);
	return 0;
}

int DM365DmaiCapture::closeCamera()
{
	if (hCapture) {
		mDebug("deleting capture handle");
		Capture_delete(hCapture);
		hCapture = NULL;
	}

	/* Clean up the thread before exiting */
	if (bufTab) {
		mDebug("deleting capture buffer tab");
		BufTab_delete(bufTab);
		bufTab = NULL;
	}
	return 0;
}

int DM365DmaiCapture::configureResizer(void)
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

RawBuffer DM365DmaiCapture::createNewRawBuffer(Buffer_Handle hBuf)
{
	RawBuffer newbuf = DmaiBuffer("video/x-raw-yuv", hBuf, this);
	newbuf.addBufferParameter("v4l2PixelFormat", (int)outPixFormat);
	newbuf.addBufferParameter("fps", 30);
	bufferPool.insert(hBuf, newbuf);
	return newbuf;
}

int DM365DmaiCapture::configurePreviewer()
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

