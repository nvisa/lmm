#ifndef V4L2OUTPUT_H
#define V4L2OUTPUT_H

#include <baselmmoutput.h>

#include <QMap>

struct Display_Object;
struct BufTab_Object;
struct _Buffer_Object;
typedef struct _Buffer_Object *Buffer_Handle;
typedef struct BufTab_Object *BufTab_Handle;
typedef struct Display_Object *Display_Handle;
class RawBuffer;

class V4l2Output : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit V4l2Output(QObject *parent = 0);
	virtual int outputBuffer(RawBuffer *buf);
	virtual int start();
	virtual int stop();
	void aboutDeleteBuffer(RawBuffer *buf);
signals:
public slots:
protected:
	Display_Handle hDisplay;
	BufTab_Handle hDispBufTab;
	QMap<Buffer_Handle, RawBuffer *> bufferPool;
};

#endif // V4L2OUTPUT_H
