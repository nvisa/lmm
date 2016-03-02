#ifndef SIMPLE1080PSTREAMER_H
#define SIMPLE1080PSTREAMER_H

#include <lmm/players/basestreamer.h>

class SonyImx;
class H264Encoder;
class TextOverlay;
class SeiInserter;
class DM365CameraInput;

class Simple1080pStreamer : public BaseStreamer
{
	Q_OBJECT
public:
	explicit Simple1080pStreamer(QObject *parent = 0);

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

#endif // SIMPLE1080PSTREAMER_H
