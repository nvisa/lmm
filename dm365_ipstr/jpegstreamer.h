#ifndef JPEGSTREAMER_H
#define JPEGSTREAMER_H

#include <lmm/players/basestreamer.h>
#include <lmm/interfaces/imagesnapshotinterface.h>

class BufferQueue;

class JpegStreamer : public BaseStreamer, ImageSnapshotInterface
{
	Q_OBJECT
public:
	explicit JpegStreamer(int bufferCount = 10, QObject *parent = 0);

	QList<RawBuffer> getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount);
signals:

public slots:
protected:
	int pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf);

	BufferQueue *que1;
};

#endif // JPEGSTREAMER_H
