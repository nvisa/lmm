#ifndef H264ENCODER_H
#define H264ENCODER_H

#include <lmm/dmai/dmaiencoder.h>

struct ROI_Interface;
struct IH264VENC_Params;
struct IH264VENC_DynamicParams;
struct VUIParamBuffer;

#include <QRect>

class H264Encoder : public DmaiEncoder
{
	Q_OBJECT
public:
	enum MotionVectors {
		MV_NONE,
		MV_SEI,
		MV_OVERLAY,
		MV_BOTH,
	};

	explicit H264Encoder(QObject *parent = 0);
	int flush();

	bool isPacketized();
	void setPacketized(bool value);
	int enablePictureTimingSei(bool enable);
	int setProfile(int v) { profileId = v; return 0; }
	int getProfile() { return profileId; }
	void setRoiMark(bool mark) { markRoi = mark; }
	bool getRoiMark() { return markRoi; }
	int setRoi(QRect rect, bool mark) { roiRect = rect; markRoi = mark; return 0; }
	QRect getRoi() { return roiRect; }
	int generateMetadata(bool v);
	int useGeneratedMetadata(bool v);
	void * getMetadata() { return frameinfoInterface; }
	void setMetadata(void * data);
	virtual void setFrameRate(float fps);
	void setMotionVectorExtraction(MotionVectors mv) { mVecs = mv; }
	MotionVectors getMotionVectorExtraction() { return mVecs; }
	void setSeiEnabled(bool value);

	/* sei information */
	void setSeiField(const QByteArray ba);

	IH264VENC_DynamicParams * getDynamicParams() { return dynH264Params; }
	int setDynamicParamsNextLoop(bool v) { setDynamicParams = v; return 0; }
signals:
	
public slots:
protected:
	void calculateFps(const RawBuffer buf);
private:
	int seiBufferSize;
	IH264VENC_DynamicParams *dynH264Params;
	QByteArray customSeiData;
	bool setDynamicParams;

	int enableBufSei;
	int enableVUIParams;
	int enablePicTimSei;
	int timeStamp;
	float encodeFps;
	struct VUIParamBuffer *vuiparambuf;
	int profileId;
	QRect roiRect;
	bool markRoi;
	bool genMetadata;
	bool useMetadata;
	Buffer_Handle metadataBuf[3];
	void *frameinfoInterface;
	MotionVectors mVecs;
	int motVectSize;
	bool packetized;

	int encode(Buffer_Handle buffer, const RawBuffer source);
	int startCodec(bool alloc = true);
	int stopCodec();
	int createSeiData(QByteArray *ba, const RawBuffer source);
	int insertSeiData(int seiDataOffset, Buffer_Handle hDstBuf, RawBuffer source);
	/* codec parameters api */
	int setDefaultParams(IH264VENC_Params *params);
	int setParamsProfile1(IH264VENC_Params *params);
	int setParamsProfile2(IH264VENC_Params *params);
	int setParamsProfile3(IH264VENC_Params *params);
	int setDefaultDynamicParams(IH264VENC_Params *params);
	int setDynamicParamsProfile1(IH264VENC_Params *params);
	int setDynamicParamsProfile2(IH264VENC_Params *params);
	int setDynamicParamsProfile3(IH264VENC_Params *params);
	ROI_Interface *roiParameter(uchar *vdata);
};

#endif // H264ENCODER_H
