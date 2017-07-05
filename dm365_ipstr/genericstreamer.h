#ifndef GENERICSTREAMER_H
#define GENERICSTREAMER_H

#include <ecl/lmmprocessbus.h>
#include <lmm/dm365/ipcamerastreamer.h>

class DM365CameraInput;
class MetadataGenerator;
class ApplicationSettings;
class StreamControlElementInterface;
class TextOverlay;

class GenericStreamer : public BaseStreamer, public LmmPBusInterface
{
	Q_OBJECT
public:
	explicit GenericStreamer(QObject *parent = 0);

	virtual QList<RawBuffer> getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount);
signals:

public slots:
protected:
	void postInitPipeline(BaseLmmPipeline *p);
	virtual int pipelineOutput(BaseLmmPipeline *p, const RawBuffer &);
	QVariant getRtspStats(const QString &fld);
	int getWdogKey();

	TextOverlay * createTextOverlay(const QString &elementName, ApplicationSettings *s);
	H264Encoder * createH264Encoder(const QString &elementName, ApplicationSettings *s, int ch, int w0, int h0);
	JpegEncoder * createJpegEncoder(const QString &elementName, ApplicationSettings *s, int ch, int w0, int h0);

	/* elements */
	DM365CameraInput *camIn;
	TextOverlay *mainTextOverlay;

	QHash<BaseLmmPipeline *, QList<int> > streamControl;
	QHash<BaseLmmPipeline *, QList<StreamControlElementInterface *> > streamControlElement;
	QList<RtpTransmitter *> transmitters;
	QList<MetadataGenerator *> metaGenerators;
	BaseRtspServer *rtsp;
	LmmProcessBus *pbus;
	int lastIrqk;
	int lastIrqkSource;
	int wdogimpl;
	QElapsedTimer lockCheckTimer;
	bool noRtspContinueSupport;

	// LmmPBusInterface interface
public:
	virtual QString getProcessName();
	virtual int getInt(const QString &fld);
	virtual QString getString(const QString &fld);
	virtual QVariant getVariant(const QString &fld);
	virtual int setInt(const QString &fld, qint32 val);
	virtual int setString(const QString &fld, const QString &val);
	virtual int setVariant(const QString &fld, const QVariant &val);
};

#endif // GENERICSTREAMER_H
