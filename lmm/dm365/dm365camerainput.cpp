#include "dm365camerainput.h"
#include "dmai/dmaibuffer.h"
#include "streamtime.h"
#include "tools/videoutils.h"
#include "hardwareoperations.h"
#include "rawbuffer.h"
#include "debug.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/priv/_Buffer.h>

#include <media/davinci/imp_previewer.h>
#include <media/davinci/imp_resizer.h>
#include <media/davinci/dm365_ipipe.h>

#define RESIZER_DEVICE   "/dev/davinci_resizer"
#define PREVIEWER_DEVICE "/dev/davinci_previewer"

#define V4L2_STD_720P_30        ((v4l2_std_id)(0x0100000000000000ULL))
#define V4L2_STD_720P_60        ((v4l2_std_id)(0x0004000000000000ULL))
#define V4L2_STD_1080P_60       ((v4l2_std_id)(0x0040000000000000ULL))

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


/* ISIF registers relative offsets */
#define SYNCEN					0x00
#define MODESET					0x04
#define HDW						0x08
#define VDW						0x0c
#define PPLN					0x10
#define LPFR					0x14
#define SPH						0x18
#define LNH						0x1c
#define SLV0					0x20
#define SLV1					0x24
#define LNV						0x28
#define CULH					0x2c
#define CULV					0x30
#define HSIZE					0x34
#define SDOFST					0x38
#define CADU					0x3c
#define CADL					0x40
#define LINCFG0					0x44
#define LINCFG1					0x48
#define CCOLP					0x4c
#define CRGAIN 					0x50
#define CGRGAIN					0x54
#define CGBGAIN					0x58
#define CBGAIN					0x5c
#define COFSTA					0x60
#define FLSHCFG0				0x64
#define FLSHCFG1				0x68
#define FLSHCFG2				0x6c
#define VDINT0					0x70
#define VDINT1					0x74
#define VDINT2					0x78
#define MISC 					0x7c
#define CGAMMAWD				0x80
#define REC656IF				0x84
#define CCDCFG					0x88

#define IPIPE_SRC_EN            (0x0000)
#define IPIPE_SRC_MODE          (0x0004)
#define IPIPE_SRC_FMT           (0x0008)
#define IPIPE_SRC_COL           (0x000C)
#define IPIPE_SRC_VPS           (0x0010)
#define IPIPE_SRC_VSZ           (0x0014)
#define IPIPE_SRC_HPS           (0x0018)
#define IPIPE_SRC_HSZ           (0x001C)
#define IPIPE_GCK_MMR           (0x0028)
#define IPIPE_GCK_PIX           (0x002C)

/* Resizer */
#define RSZ_SRC_EN              (0x0)
#define RSZ_SRC_MODE            (0x4)
#define RSZ_SRC_FMT0            (0x8)
#define RSZ_SRC_FMT1            (0xC)
#define RSZ_SRC_VPS             (0x10)
#define RSZ_SRC_VSZ             (0x14)
#define RSZ_SRC_HPS             (0x18)
#define RSZ_SRC_HSZ             (0x1C)
#define RSZ_DMA_RZA             (0x20)
#define RSZ_DMA_RZB             (0x24)
#define RSZ_DMA_STA             (0x28)
#define RSZ_GCK_MMR             (0x2C)
#define RSZ_RESERVED0           (0x30)
#define RSZ_GCK_SDR             (0x34)
#define RSZ_IRQ_RZA             (0x38)
#define RSZ_IRQ_RZB             (0x3C)
#define RSZ_YUV_Y_MIN           (0x40)
#define RSZ_YUV_Y_MAX           (0x44)
#define RSZ_YUV_C_MIN           (0x48)
#define RSZ_YUV_C_MAX           (0x4C)
#define RSZ_YUV_PHS             (0x50)
#define RSZ_SEQ                 (0x54)
/* Resizer Rescale Parameters */
#define RSZ_EN_A                (0x58)
#define RSZ_EN_B                (0xE8)
/* offset of the registers to be added with base register of
   either RSZ0 or RSZ1
*/
#define RSZ_MODE                (0x4)
#define RSZ_420                 (0x8)
#define RSZ_I_VPS               (0xC)
#define RSZ_I_HPS               (0x10)
#define RSZ_O_VSZ               (0x14)
#define RSZ_O_HSZ               (0x18)
#define RSZ_V_PHS_Y             (0x1C)
#define RSZ_V_PHS_C             (0x20)
#define RSZ_V_DIF               (0x24)
#define RSZ_V_TYP               (0x28)
#define RSZ_V_LPF               (0x2C)
#define RSZ_H_PHS               (0x30)
#define RSZ_H_PHS_ADJ           (0x34)
#define RSZ_H_DIF               (0x38)
#define RSZ_H_TYP               (0x3C)
#define RSZ_H_LPF               (0x40)
#define RSZ_DWN_EN              (0x44)
#define RSZ_DWN_AV              (0x48)

/* Resizer RGB Conversion Parameters */
#define RSZ_RGB_EN              (0x4C)
#define RSZ_RGB_TYP             (0x50)
#define RSZ_RGB_BLD             (0x54)

/* Resizer External Memory Parameters */
#define RSZ_SDR_Y_BAD_H         (0x58)
#define RSZ_SDR_Y_BAD_L         (0x5C)
#define RSZ_SDR_Y_SAD_H         (0x60)
#define RSZ_SDR_Y_SAD_L         (0x64)
#define RSZ_SDR_Y_OFT           (0x68)
#define RSZ_SDR_Y_PTR_S         (0x6C)
#define RSZ_SDR_Y_PTR_E         (0x70)
#define RSZ_SDR_C_BAD_H         (0x74)
#define RSZ_SDR_C_BAD_L         (0x78)
#define RSZ_SDR_C_SAD_H         (0x7C)
#define RSZ_SDR_C_SAD_L         (0x80)
#define RSZ_SDR_C_OFT           (0x84)
#define RSZ_SDR_C_PTR_S         (0x88)
#define RSZ_SDR_C_PTR_E         (0x8C)


class BaseHwConfig
{
public:
	BaseHwConfig()
	{

	}

	/* these are parameters for desired output image */
	int width;
	int height;
	int linelen;

	virtual int setup()
	{
		/* setup with a paused pipeline */
		enablePipeline(false);

		int err = setupISIF();
		if (err)
			return err;

		err = setupIPIPEIF();
		if (err)
			return err;

		err = setupIPIPE();
		if (err)
			return err;

		err = setupResizer();
		if (err)
			return err;

		return 0;
	}

	virtual int enablePipeline(bool en)
	{
		enableISIF(en);
		enableIPIPEIF(en);
		enableIPIPE(en);
		enableResizer(en, en ? 0x01 : 0x00); //enable only A channel
		return 0;
	}
protected:
	virtual int setupISIF() = 0;
	virtual int setupIPIPEIF() = 0;
	virtual int setupIPIPE() = 0;
	virtual int setupResizer() = 0;

	virtual int enableISIF(bool en) = 0;
	virtual int enableIPIPEIF(bool en) = 0;
	virtual int enableIPIPE(bool en) = 0;
	virtual int enableResizer(bool en, int ch) = 0;
};

class YPbPrConfig : public BaseHwConfig
{
public:
	YPbPrConfig()
		: BaseHwConfig()
	{

	}

	~YPbPrConfig()
	{
		enablePipeline(false);
	}

protected:
	int enableISIF(bool en)
	{
		HardwareOperations hop;
		int err = hop.map(0x1c71000);
		if (err)
			return err;
		hop.write(SYNCEN, en ? 1 : 0);
		hop.unmap();
		return 0;
	}

	int enableIPIPEIF(bool en)
	{
		HardwareOperations hop;
		int err = hop.map(0x1c71200);
		if (err)
			return err;
		hop.write(0x0, en ? 1 : 0);
		hop.unmap();
		return 0;
	}

	int enableIPIPE(bool en)
	{
		HardwareOperations hop;
		int err = hop.map(0x1c70800);
		if (err)
			return err;
		hop.write(IPIPE_GCK_MMR, 0x1);
		hop.write(IPIPE_GCK_PIX, 0xe);
		hop.write(IPIPE_SRC_EN, en ? 1 : 0);
		hop.unmap();
		return 0;
	}

	int enableResizer(bool en, int ch)
	{
		HardwareOperations hop;
		int err = hop.map(0x1c70400);
		if (err)
			return err;
		hop.write(RSZ_GCK_MMR, 0x1);
		hop.write(RSZ_SRC_EN, en ? 1 : 0);
		if (ch & 0x1)
			hop.write(RSZ_EN_A, 0x1);
		else
			hop.write(RSZ_EN_A, 0x0);
		if (ch & 0x2)
			hop.write(RSZ_EN_B, 0x1);
		else
			hop.write(RSZ_EN_B, 0x0);
		hop.unmap();
		return 0;
	}

	int setupISIF()
	{
		HardwareOperations hop;
		int err = hop.map(0x1c71000);
		if (err)
			return err;
		hop.write(MODESET, (1 << 12)); //16-bit YPbPr
		hop.write(HDW, 0); //not used when input
		hop.write(VDW, 0); //not used when input
		hop.write(PPLN, 0); //not used when input
		hop.write(LPFR, 0); //not used when input
		hop.write(SPH, 0);
		hop.write(LNH, width - 1);
		hop.write(SLV0, 0);
		hop.write(SLV1, 0);
		hop.write(LNV, height - 1);
		hop.write(HSIZE, ((((width * 2) + 31) & 0xffffffe0) >> 5));
		hop.write(VDINT0, height - 1); //FIXME: check this
		hop.write(VDINT1, height / 2); //FIXME: check this

		hop.unmap();
		return 0;
	}

	int setupIPIPEIF()
	{
		/* no IPIPEIF in this mode */
		return 0;
	}

	int setupIPIPE()
	{
		HardwareOperations hop;
		int err = hop.map(0x1c70800);
		if (err)
			return err;
		hop.write(IPIPE_GCK_MMR, 0x1);
		hop.write(IPIPE_SRC_MODE, 0);
		hop.write(IPIPE_SRC_FMT, 3);
		hop.write(IPIPE_SRC_VPS, 1); //FIXME: check this
		hop.write(IPIPE_SRC_VSZ, height - 1);
		hop.write(IPIPE_SRC_HPS, 0);
		hop.write(IPIPE_SRC_HSZ, width - 1);

		hop.unmap();
		return 0;
	}

	virtual int setupResizer()
	{
		HardwareOperations hop;
		int err = hop.map(0x1c70400);
		if (err)
			return err;
		hop.write(RSZ_GCK_MMR, 0x1);
		hop.write(RSZ_SRC_MODE, 0x0);
		hop.write(RSZ_SRC_FMT0, 0x0);
		hop.write(RSZ_SRC_FMT1, 0x0);
		hop.write(RSZ_SRC_VPS, 0x0);
		hop.write(RSZ_SRC_VSZ, height - 1);
		hop.write(RSZ_SRC_HPS, 0);
		hop.write(RSZ_SRC_HSZ, width - 1);
		hop.write(RSZ_GCK_SDR, 0x1);
		hop.write(RSZ_SEQ, 0x0);
		int off = RSZ_EN_A;
		hop.write(off + RSZ_MODE, 0x0); //continuous mode
		hop.write(off + RSZ_420, 0x03); //enable 420 conversion
		hop.write(off + RSZ_I_VPS, 0x0);
		hop.write(off + RSZ_I_HPS, 0x0);
		hop.write(off + RSZ_O_VSZ, height - 1);
		hop.write(off + RSZ_O_HSZ, width - 1);
		hop.write(off + RSZ_V_PHS_Y, 0);
		hop.write(off + RSZ_V_PHS_C, 0);
		hop.write(off + RSZ_V_DIF, 256);
		hop.write(off + RSZ_V_TYP, 0);
		hop.write(off + RSZ_H_DIF, 256);
		hop.write(off + RSZ_H_TYP, 0);
		hop.write(off + RSZ_H_PHS, 0);
		hop.write(off + RSZ_H_PHS_ADJ, 0);
		hop.write(off + RSZ_DWN_EN, 0x0); //no down scaling
		hop.write(off + RSZ_RGB_EN, 0x0); //no rgb conversion

		/* BAD|SAD{H|L} registers should be adjusted by kernel driver */
		hop.write(off + RSZ_SDR_Y_OFT, linelen);
		hop.write(off + RSZ_SDR_Y_PTR_S, 0x0); //can be used for cropping
		hop.write(off + RSZ_SDR_Y_PTR_E, height);
		hop.write(off + RSZ_SDR_C_OFT, linelen);
		hop.write(off + RSZ_SDR_C_PTR_S, 0x0); //can be used for cropping
		hop.write(off + RSZ_SDR_C_PTR_E, height / 2);

		hop.unmap();
		return 0;
	}
};

DM365CameraInput::DM365CameraInput(QObject *parent) :
	V4l2Input(parent)
{
	inputType = COMPONENT;
	/* these capture w and h are defaults, v4l2input overrides them in start */
	inputCaptureWidth = 1280;
	inputCaptureHeight = 720;
	captureWidth = 1280;
	captureHeight = 720;
	captureWidth2 = 352;
	captureHeight2 = 288;
	pixFormat = V4L2_PIX_FMT_NV12;
	hCapture = NULL;
	captureBufferCount = 8;
	addNewOutputChannel();

	ch1HorFlip = false;
	ch1VerFlip = false;
	ch2HorFlip = false;
	ch2VerFlip = false;

	inputFps = outputFps = 30;
}

void DM365CameraInput::setInputFps(float fps)
{
	inputFps = fps;
}

void DM365CameraInput::setOutputFps(float fps)
{
	outputFps = fps;
}

void DM365CameraInput::aboutToDeleteBuffer(const RawBufferParameters *params)
{
	v4l2_buffer *buffer = (v4l2_buffer *)params->v4l2Buffer;
	useLock.lock();
	if (--useCount[buffer->index] == 0) {
		mInfo("buffer %p use count is zero, giving back to kernel driver", buffer);
		V4l2Input::aboutToDeleteBuffer(params);
	}
	useLock.unlock();
}

int DM365CameraInput::setSize(int ch, QSize sz)
{
	if (ch == 0) {
		captureWidth = sz.width();
		captureHeight = sz.height();
		inputCaptureWidth = sz.width();
		inputCaptureHeight = sz.height();
	} else {
		captureWidth2 = sz.width();
		captureHeight2 = sz.height();
	}
	return 0;
}

int DM365CameraInput::setInputSize(QSize sz)
{
	inputCaptureWidth = sz.width();
	inputCaptureHeight = sz.height();
	return 0;
}

QSize DM365CameraInput::getSize(int ch)
{
	if (ch)
		return QSize(captureWidth2, captureHeight2);
	return QSize(captureWidth, captureHeight);
}

void DM365CameraInput::setVerticalFlip(int ch, bool flip)
{
	if (ch)
		ch2VerFlip = flip;
	else
		ch1VerFlip = flip;
}

void DM365CameraInput::setNonStdOffsets(int vbp, int hbp)
{
	nonStdInput.hbp = hbp;
	nonStdInput.vbp = vbp;
	nonStdInput.enabled = (vbp + hbp) ? true : false;
}

void DM365CameraInput::setHorizontalFlip(int ch, bool flip)
{
	if (ch)
		ch2HorFlip = flip;
	else
		ch1HorFlip = flip;
}

QList<QVariant> DM365CameraInput::extraDebugInfo()
{
	QList<QVariant> list;
	int sum = 0;
	useLock.lock();
	for (int i = 0; i < useCount.size(); i++)
		sum += useCount[i];
	useLock.unlock();
	list << sum;
	return list;
}

void DM365CameraInput::calculateFps(const RawBuffer buf)
{
	if (buf.constPars()->videoWidth == captureWidth &&
			buf.constPars()->videoHeight == captureHeight)
		return BaseLmmElement::calculateFps(buf);
}

int DM365CameraInput::openCamera()
{
	struct v4l2_capability cap;
	struct v4l2_input input;
	int width = captureWidth, height = captureHeight;
	int width2 = captureWidth2, height2 = captureHeight2;
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
	mDebug("setting input index to '%d'", inputIndex);
	inputIndex = input.index;
	err = setInput(&input);
	if (err && inputType != COMPOSITE)
		return err;

	v4l2_std_id std_id = V4L2_STD_1080P_60;
	if (inputType == COMPOSITE || inputType == S_VIDEO)
		std_id = V4L2_STD_PAL;
	err = setStandard(&std_id);
	if (err)
		return err;

	if (outputFps != inputFps)
		fpsWorkaround();

	err = queryCapabilities(&cap);
	if (err)
		return err;

	adjustCropping(inputCaptureWidth, inputCaptureHeight);
	err = setFormat(pixFormat, width, height, inputType == COMPOSITE);
	if (err)
		return err;

	/* buffer allocation */
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	gfxAttrs.dim.x = 0;
	gfxAttrs.dim.y = 0;
	int bufSize = VideoUtils::getFrameSize(
				pixFormat, (width + width2), (height + height2));
	if (pixFormat == V4L2_PIX_FMT_UYVY)
		gfxAttrs.colorSpace = ColorSpace_UYVY;
	else if (pixFormat == V4L2_PIX_FMT_NV12)
		gfxAttrs.colorSpace = ColorSpace_YUV420PSEMI;
	else
		return -EINVAL;

	for (uint i = 0; i < captureBufferCount; i++) {
		gfxAttrs.dim.width = width;
		gfxAttrs.dim.height = height;
		gfxAttrs.dim.lineLength = VideoUtils::getLineLength(pixFormat, width);
		gfxAttrs.bAttrs.reference = 0;
		Buffer_Handle h = Buffer_create(bufSize, BufferGfx_getBufferAttrs(&gfxAttrs));
		if (!h) {
			mDebug("unable to create %d capture buffers with size %d", captureBufferCount, bufSize);
			return -ENOMEM;
		}
		Buffer_setNumBytesUsed(h, bufSize);
		srcBuffers << h;

		/* create reference buffers for rsz A channel */
		gfxAttrs.dim.width = width;
		gfxAttrs.dim.height = height;
		gfxAttrs.dim.lineLength = VideoUtils::getLineLength(pixFormat, width);
		gfxAttrs.bAttrs.reference = true;
		Buffer_Handle h2 = Buffer_create(0, BufferGfx_getBufferAttrs(&gfxAttrs));
		if (!h2) {
			mDebug("unable to create capture buffers");
			return -ENOMEM;
		}
		Buffer_setSize(h2, VideoUtils::getFrameSize(pixFormat, width, height));
		Buffer_setUserPtr(h2, Buffer_getUserPtr(h));
		refBuffersA << h2;

		/* create reference buffers for rsz B channel */
		gfxAttrs.bAttrs.reference = true;
		gfxAttrs.dim.width = width2;
		gfxAttrs.dim.height = height2;
		gfxAttrs.dim.lineLength = VideoUtils::getLineLength(pixFormat, width2);
		Buffer_Handle h3 = Buffer_create(0, BufferGfx_getBufferAttrs(&gfxAttrs));
		if (!h3) {
			mDebug("unable to create capture buffers");
			return -ENOMEM;
		}
		Buffer_setSize(h3, VideoUtils::getFrameSize(pixFormat, width2, height2));
		/*
		 * NOTE:
		 * we cannot simple set user pointer like this:
		 *		Buffer_setUserPtr(h3, Buffer_getUserPtr(h) + Buffer_getSize(h3));
		 * This is because cmem allocator will not be able to resolve physical
		 * address from this. Workaround is to use these buffers with a proper
		 * offset
		 **/
		Buffer_setUserPtr(h3, Buffer_getUserPtr(h));
		h3->physPtr += Buffer_getSize(h2);
		h3->userPtr += Buffer_getSize(h2);
		refBuffersB << h3;

		useCount << 0;
	}
	err = allocBuffers();
	if (err) {
		mDebug("unable to allocate driver buffers with error %d", err);
		return err;
	}

	mDebug("buffers are allocated, starting streaming");
	err = startStreaming();
	if (nonStdInput.enabled)
		setupNonStdMode();
	return err;
}

int DM365CameraInput::openCamera2()
{
	int width = captureWidth, height = captureHeight;
	int width2 = captureWidth2, height2 = captureHeight2;
	int err = 0;

	err = openDeviceNode();
	if (err)
		return err;

	BaseHwConfig *config = new YPbPrConfig;
	config->width = width;
	config->height = height;
	config->linelen = width;
	err = config->setup();
	if (err)
		return err;

	err = setFormat(pixFormat, width, height, inputType == COMPOSITE);
	if (err)
		return err;

	/* buffer allocation */
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	gfxAttrs.dim.x = 0;
	gfxAttrs.dim.y = 0;
	int bufSize = VideoUtils::getFrameSize(
				pixFormat, (width + width2), (height + height2));
	if (pixFormat == V4L2_PIX_FMT_UYVY)
		gfxAttrs.colorSpace = ColorSpace_UYVY;
	else if (pixFormat == V4L2_PIX_FMT_NV12)
		gfxAttrs.colorSpace = ColorSpace_YUV420PSEMI;
	else
		return -EINVAL;

	for (uint i = 0; i < captureBufferCount; i++) {
		gfxAttrs.dim.width = width;
		gfxAttrs.dim.height = height;
		gfxAttrs.dim.lineLength = VideoUtils::getLineLength(pixFormat, width);
		gfxAttrs.bAttrs.reference = 0;
		Buffer_Handle h = Buffer_create(bufSize, BufferGfx_getBufferAttrs(&gfxAttrs));
		if (!h) {
			mDebug("unable to create %d capture buffers with size %d", captureBufferCount, bufSize);
			return -ENOMEM;
		}
		Buffer_setNumBytesUsed(h, bufSize);
		srcBuffers << h;

		/* create reference buffers for rsz A channel */
		gfxAttrs.dim.width = width;
		gfxAttrs.dim.height = height;
		gfxAttrs.dim.lineLength = VideoUtils::getLineLength(pixFormat, width);
		gfxAttrs.bAttrs.reference = true;
		Buffer_Handle h2 = Buffer_create(0, BufferGfx_getBufferAttrs(&gfxAttrs));
		if (!h2) {
			mDebug("unable to create capture buffers");
			return -ENOMEM;
		}
		Buffer_setSize(h2, VideoUtils::getFrameSize(pixFormat, width, height));
		Buffer_setUserPtr(h2, Buffer_getUserPtr(h));
		refBuffersA << h2;

		/* create reference buffers for rsz B channel */
		gfxAttrs.bAttrs.reference = true;
		gfxAttrs.dim.width = width2;
		gfxAttrs.dim.height = height2;
		gfxAttrs.dim.lineLength = VideoUtils::getLineLength(pixFormat, width2);
		Buffer_Handle h3 = Buffer_create(0, BufferGfx_getBufferAttrs(&gfxAttrs));
		if (!h3) {
			mDebug("unable to create capture buffers");
			return -ENOMEM;
		}
		Buffer_setSize(h3, VideoUtils::getFrameSize(pixFormat, width2, height2));
		/*
		 * NOTE:
		 * we cannot simple set user pointer like this:
		 *		Buffer_setUserPtr(h3, Buffer_getUserPtr(h) + Buffer_getSize(h3));
		 * This is because cmem allocator will not be able to resolve physical
		 * address from this. Workaround is to use these buffers with a proper
		 * offset
		 **/
		Buffer_setUserPtr(h3, Buffer_getUserPtr(h));
		h3->physPtr += Buffer_getSize(h2);
		h3->userPtr += Buffer_getSize(h2);
		refBuffersB << h3;

		useCount << 0;
	}
	err = allocBuffers();
	if (err) {
		mDebug("unable to allocate driver buffers with error %d", err);
		return err;
	}

	mDebug("buffers are allocated, starting streaming");
	err = startStreaming();

	config->enablePipeline(true);

	return err;
}

int DM365CameraInput::closeCamera()
{
	stopStreaming();
	close(fd);
	if (rszFd > 0)
		close(rszFd);
	if (preFd > 0)
		close(preFd);
	qDeleteAll(v4l2buf);
	v4l2buf.clear();
	userptr.clear();
	fd = rszFd = preFd = -1;
	clearDmaiBuffers();
	bufsFree.acquire(bufsFree.available());
	mInfo("capture closed");

	return 0;
}

int DM365CameraInput::setupISIF(DM365CameraInput::ISIFMode mode)
{
	HardwareOperations hop;
	hop.map(0x1c71000);
	hop.write(0x08, mode.hdw);
	hop.write(0x0c, mode.vdw);
	hop.write(0x10, mode.ppln);
	hop.write(0x14, mode.lpfr);
	hop.write(0x18, mode.sph);
	hop.write(0x1c, mode.lnh);
	hop.write(0x20, mode.slv0);
	hop.write(0x24, mode.slv1);
	hop.write(0x2c, mode.culh);
	hop.write(0x30, mode.culv);
	hop.write(0x34, mode.hsize);
	hop.write(0x38, mode.sdofst);
	hop.write(0x3c, mode.cadu);
	hop.write(0x40, mode.cadl);
	hop.unmap();
	return 0;
}

int DM365CameraInput::setupIPIPE(DM365CameraInput::IPIPEMode mode)
{
	HardwareOperations hop;
	int err = hop.map(0x1c70800);
	if (err) {
		mDebug("error mapping IPIPE registers");
		return err;
	}
	hop.write(0x10, mode.vps);
	hop.write(0x18, mode.hps);
	hop.unmap();
	return 0;
}

int DM365CameraInput::setupNonStdMode()
{
	mDebug("adjusting for non-standard input mode: hbp=%d vbp=%d", nonStdInput.hbp
		   , nonStdInput.vbp);
	/* ISIF registers are not used in chained mode */
	/* IPIPEIF is bypassed/not used in chained mode ??? */
	IPIPEMode mode;
	mode.hps = nonStdInput.hbp;
	mode.vps = nonStdInput.vbp;
	setupIPIPE(mode);
	return 0;
}


int DM365CameraInput::fpsWorkaround()
{
	struct v4l2_streamparm streamparam;
	Dmai_clear(streamparam);
	streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	streamparam.parm.capture.timeperframe.numerator = 1;
	streamparam.parm.capture.timeperframe.denominator = 25;
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
		return -errno;
	}

	/* The driver may return less buffers than requested */
	if (req.count < NUM_CAPTURE_BUFS || !req.count) {
		mDebug("Insufficient device driver buffer memory");
		return -errno;
	}

	for (int i = 0; i < (int)req.count; i++) {
		Buffer_Handle hBuf = srcBuffers[i];
		userptr << (char *)Buffer_getUserPtr(hBuf);
		v4l2buf << new struct v4l2_buffer;
		memset(v4l2buf[i], 0, sizeof(v4l2_buffer));
		v4l2buf[i]->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		v4l2buf[i]->memory = V4L2_MEMORY_USERPTR;
		v4l2buf[i]->index = i;
		v4l2buf[i]->m.userptr = (int)Buffer_getUserPtr(hBuf);
		v4l2buf[i]->length = Buffer_getNumBytesUsed(hBuf);

		/* Queue buffer in device driver */
		if (ioctl(fd, VIDIOC_QBUF, v4l2buf[i]) == -1) {
			mDebug("VIODIC_QBUF failed (%s)", strerror(errno));
			return -errno;
		}
	}
	bufsFree.release(NUM_CAPTURE_BUFS - 1);

	return 0;
}

void DM365CameraInput::clearDmaiBuffers()
{
	foreach (Buffer_Handle h, refBuffersA)
		Buffer_delete(h);
	foreach (Buffer_Handle h, refBuffersB)
		Buffer_delete(h);
	foreach (Buffer_Handle h, srcBuffers)
		Buffer_delete(h);
	refBuffersA.clear();
	refBuffersB.clear();
	srcBuffers.clear();
	useLock.lock();
	useCount.clear();
	useLock.unlock();
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
		if (errno != EAGAIN)
			mDebug("VIDIOC_DQBUF failed with err %d", -errno);
		return NULL;
	}

	return v4l2buf[buffer.index];
}

int DM365CameraInput::processBuffer(v4l2_buffer *buffer)
{
	int passed = timing.restart();
	mInfo("captured %p, time is %lld, passed %d", buffer, streamTime->getFreeRunningTime(), passed);
	Buffer_Handle dbufa = refBuffersA[buffer->index];
	Buffer_Handle dbufb = refBuffersB[buffer->index];

	RawBuffer newbuf = DmaiBuffer("video/x-raw-yuv", dbufa, this);
	newbuf.pars()->v4l2Buffer = (quintptr *)buffer;
	newbuf.pars()->captureTime = streamTime->getCurrentTime()
							  - (captureBufferCount - 1) * 1000 * 1000 / outputFps;
	newbuf.pars()->v4l2PixelFormat = pixFormat;
	newbuf.pars()->fps = outputFps;
	newbuf.pars()->videoWidth = captureWidth;
	newbuf.pars()->videoHeight = captureHeight;

	RawBuffer sbuf = DmaiBuffer("video/x-raw-yuv", dbufb, this);
	sbuf.pars()->v4l2Buffer = (quintptr *)buffer;
	sbuf.pars()->captureTime = streamTime->getCurrentTime()
							  - (captureBufferCount - 1) * 1000 * 1000 / outputFps;
	sbuf.pars()->v4l2PixelFormat = pixFormat;
	sbuf.pars()->fps = outputFps;
	sbuf.pars()->videoWidth = captureWidth2;
	sbuf.pars()->videoHeight = captureHeight2;

	useLock.lock();
	useCount[buffer->index] += 2;
	useLock.unlock();

	newOutputBuffer(0, newbuf);
	newOutputBuffer(1, sbuf);

	if (passed > 35)
		mInfo("late capture: %d", passed);

	return 0;
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
	rsz_cont_config.output1.h_flip = ch1HorFlip ? 1 : 0;
	rsz_cont_config.output1.v_flip = ch1VerFlip ? 1 : 0;

	if (captureWidth2 == 0 || captureHeight2 == 0)
		rsz_cont_config.output2.enable = 0;
	else
		rsz_cont_config.output2.enable = 1;
	rsz_cont_config.output2.pix_fmt = IPIPE_YUV420SP;
	rsz_cont_config.output2.width = captureWidth2;
	rsz_cont_config.output2.height = captureHeight2;
	rsz_cont_config.output2.vst_y = 0;  //line offset for y
	rsz_cont_config.output2.vst_c = 0;  //line offset for c
	rsz_cont_config.output2.h_flip = ch2HorFlip ? 1 : 0;
	rsz_cont_config.output2.v_flip = ch2VerFlip ? 1 : 0;

	/* interpolation types */
	rsz_cont_config.output2.v_typ_y = RSZ_INTP_CUBIC;
	rsz_cont_config.output2.v_typ_c = RSZ_INTP_CUBIC;
	rsz_cont_config.output2.h_typ_y = RSZ_INTP_CUBIC;
	rsz_cont_config.output2.h_typ_c = RSZ_INTP_CUBIC;

	/* intensities */
	rsz_cont_config.output2.v_lpf_int_y = 0;
	rsz_cont_config.output2.v_lpf_int_c = 0;
	rsz_cont_config.output2.h_lpf_int_y = 0;
	rsz_cont_config.output2.h_lpf_int_c = 0;

	rsz_cont_config.output2.en_down_scale = 0;
	rsz_cont_config.output2.h_dscale_ave_sz = IPIPE_DWN_SCALE_1_OVER_4;
	rsz_cont_config.output2.v_dscale_ave_sz = IPIPE_DWN_SCALE_1_OVER_4; //BUG???
	rsz_cont_config.output2.user_y_ofst = 0;
	rsz_cont_config.output2.user_c_ofst = 0;

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
