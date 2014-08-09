#ifndef LMMCOMMON_H
#define LMMCOMMON_H

#include <QObject>

namespace Lmm {
	enum VideoOutput {
		COMPOSITE,
		COMPONENT,
		PRGB,
		YCC8,
		YCC16
	};
	enum CodecType {
		CODEC_MPEG2,
		CODEC_MPEG4,
		CODEC_H264,
		CODEC_JPEG
	};
	enum AspectRatio {
		ASPECT_RATIO_4_3,
		ASPECT_RATIO_5_4,
		ASPECT_RATIO_16_9,
		ASPECT_RATIO_16_10,
	};
	enum AudioSampleType {
		AUDIO_SAMPLE_U8,
		AUDIO_SAMPLE_S16,
		AUDIO_SAMPLE_S32,
		AUDIO_SAMPLE_FLT,
		AUDIO_SAMPLE_DBL,
		AUDIO_SAMPLE_U8P,
		AUDIO_SAMPLE_S16P,
		AUDIO_SAMPLE_S32P,
		AUDIO_SAMPLE_FLTP,
	};
}

class PlatformCommon
{
public:
	PlatformCommon()
	{

	}

	virtual void platformInit() = 0;
	virtual void platformCleanUp() = 0;
};

class QGraphicsView;
class BaseLmmPlayer;
class BaseLmmElement;
class LmmCommon : public QObject
{
	Q_OBJECT
public:
	static int init();
	static int installSignalHandlers(bool exitOnSigInt = true);
	static int registerForSignal(int signal, BaseLmmElement *el);
	static int registerForPipeSignal(BaseLmmElement *el);
	static QString getLibraryVersion();
	static QString getLiveMediaVersion();
	static QString getLibVlcVersion();
	static QString getFFmpegVersion();
	static QString getGStreamerVersion();
	static void platformInit();
	static void platformCleanUp();
signals:
	
public slots:
private:
	explicit LmmCommon(QObject *parent = 0);

	static LmmCommon inst;
	PlatformCommon *plat;
	
};

#endif // LMMCOMMON_H
