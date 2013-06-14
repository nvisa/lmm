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
	explicit H264Encoder(QObject *parent = 0);
	int flush();

	int getMaxFrameRate() { return maxFrameRate / 1000; }
	int setMaxFrameRate(int v) { maxFrameRate = v * 1000; dirty = true; return 0; }
	RateControl getBitrateControlMethod() { return rateControl; }
	int getBitrate() { return videoBitRate; }
	int setBitrateControl(RateControl v) { rateControl = v; dirty = true; return 0; }
	int setBitrate(int v) { videoBitRate = v; dirty = true; return 0; }
	int getIntraFrameInterval() { return intraFrameInterval; }
	int setIntraFrameInterval(int v) { intraFrameInterval = v; dirty = true; return 0; }
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

	/* sei information */
	void setCustomSeiFieldCount(int value);
	void setSeiField(int field, int value);

	IH264VENC_DynamicParams * getDynamicParams() { return dynH264Params; }
	int setDynamicParamsNextLoop(bool v) { setDynamicParams = v; return 0; }
signals:
	
public slots:
private:
	int seiBufferSize;
	IH264VENC_DynamicParams *dynH264Params;
	QList<int> customSeiData;
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

	int encode(Buffer_Handle buffer, const RawBuffer source);
	int startCodec();
	int stopCodec();
	int addSeiData(QByteArray *ba, const RawBuffer source);
	/* codec parameters api */
	int setDefaultParams(IH264VENC_Params *params);
	int setParamsProfile1(IH264VENC_Params *params);
	int setDefaultDynamicParams(IH264VENC_Params *params);
	int setDynamicParamsProfile1(IH264VENC_Params *params);
	ROI_Interface *roiParameter(uchar *vdata);
};

#endif // H264ENCODER_H
