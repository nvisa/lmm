#ifndef CVBSSTREAMER_H
#define CVBSSTREAMER_H

#include <lmm/players/basestreamer.h>

class SonyImx;
class H264Encoder;
class TextOverlay;
class SeiInserter;
class DM365CameraInput;

class CVBSStreamer : public BaseStreamer
{
	Q_OBJECT
public:
	explicit CVBSStreamer(QObject *parent = 0);

	virtual QList<RawBuffer> getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount);

signals:

public slots:

protected:
	virtual int startStreamer();
	virtual int pipelineOutput(BaseLmmPipeline *pipeline, const RawBuffer &buf);
	QSize getInputSize(int input);

	bool sensorMode;
	SonyImx *isen;

	/* elements */
	DM365CameraInput *camIn;
	H264Encoder *enc264High;
	TextOverlay *textOverlay;
	SeiInserter *seiInserterHigh;
};

#endif // CVBSSTREAMER_H
