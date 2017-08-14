#ifndef LMMGSTPIPELINE_H
#define LMMGSTPIPELINE_H

#include <lmm/baselmmelement.h>
#include <lmm/gstreamer/basegstcaps.h>

struct _GstAppSrc;
typedef struct _GstAppSrc GstAppSrc;
struct _GstAppSink;
typedef struct _GstAppSink GstAppSink;
struct _GstElement;
typedef struct _GstElement GstElement;
struct _GstMessage;
typedef struct _GstMessage GstMessage;
struct _GstBuffer;
typedef struct _GstBuffer GstBuffer;

class LmmGstPipeline : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit LmmGstPipeline(QObject *parent = 0);
	void setPipelineDescription(QString desc);

	int start();
	int stop();
	bool gstBusFunction(GstMessage *msg);
	int newGstBuffer(GstBuffer *buffer, GstCaps *caps, GstAppSink *sink);

	void addAppSource(const QString &name, const QString &mime);
	void addAppSink(const QString &name, const QString &mime);

	BaseGstCaps *getSourceCaps(int index);
	BaseGstCaps *getSinkCaps(int index);
	int setSourceCaps(int index, BaseGstCaps *caps);
signals:
	
public slots:
protected:
	QString pipelineDesc;
	GstElement *bin;

	QStringList sourceNames;
	QStringList sinkNames;
	QList<GstAppSrc *> sources;
	QList<GstAppSink *> sinks;
	QList<BaseGstCaps *> sourceCaps;
	QList<BaseGstCaps *> sinkCaps;

	int processBuffer(const RawBuffer &buf);
};

#endif // LMMGSTPIPELINE_H
