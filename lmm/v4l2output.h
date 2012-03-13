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
	int outputBuffer(RawBuffer *buf);
	int start();
	int stop();
	void aboutDeleteBuffer(RawBuffer *buf);
signals:
public slots:
private:
	Display_Handle hDisplay;
	BufTab_Handle hDispBufTab;
	QMap<Buffer_Handle, RawBuffer *> bufferPool;
};

#endif // V4L2OUTPUT_H
