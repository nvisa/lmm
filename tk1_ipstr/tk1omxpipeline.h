#ifndef TK1OMXPIPELINE_H
#define TK1OMXPIPELINE_H

#include <lmm/gstreamer/lmmgstpipeline.h>

class TK1OmxPipeline : public LmmGstPipeline
{
	Q_OBJECT
public:
	explicit TK1OmxPipeline(QObject *parent = 0);

signals:

public slots:
protected:
	virtual void initElements();
};

#endif // TK1OMXPIPELINE_H
