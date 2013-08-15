#ifndef LMMGSTPIPELINE_H
#define LMMGSTPIPELINE_H

#include <lmm/baselmmelement.h>

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
	void setPipelineDescription(QString desc) { pipelineDesc = desc; }
	int start();
	int stop();
	int addBuffer(RawBuffer buffer);
	bool gstBusFunction(GstMessage *msg);
	int newGstBuffer(GstBuffer *buffer);
	void setInputMimeType(QString mime) { inputMime = mime; }
	void setOutputMimeType(QString mime) { outputMime = mime; }
signals:
	
public slots:
protected:
	QString pipelineDesc;
	GstElement *bin;
	GstAppSrc *appSrc;
	GstAppSink *appSink;
	QString inputMime;
	QString outputMime;

};

#endif // LMMGSTPIPELINE_H