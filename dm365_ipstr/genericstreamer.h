#ifndef GENERICSTREAMER_H
#define GENERICSTREAMER_H

#include <lmm/dm365/ipcamerastreamer.h>

class DM365CameraInput;
class ApplicationSettings;
class StreamControlElementInterface;

class GenericStreamer : public BaseStreamer
{
	Q_OBJECT
public:
	explicit GenericStreamer(QObject *parent = 0);

	virtual QList<RawBuffer> getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount);
signals:

public slots:
protected:
	virtual int pipelineOutput(BaseLmmPipeline *p, const RawBuffer &);

	TextOverlay * createTextOverlay(const QString &elementName, ApplicationSettings *s);
	H264Encoder * createH264Encoder(const QString &elementName, ApplicationSettings *s, int ch, int w0, int h0);
	JpegEncoder * createJpegEncoder(const QString &elementName, ApplicationSettings *s, int ch, int w0, int h0);

	/* elements */
	DM365CameraInput *camIn;

	QHash<BaseLmmPipeline *, int> streamControl;
	QHash<BaseLmmPipeline *, StreamControlElementInterface *> streamControlElement;
};

#endif // GENERICSTREAMER_H
