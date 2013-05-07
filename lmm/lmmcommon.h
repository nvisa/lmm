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
	static int init(bool exitOnSigInt = true);
	static int installSignalHandlers();
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
