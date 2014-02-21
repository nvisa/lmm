#ifndef DMAIDECODER_H
#define DMAIDECODER_H

#include <lmm/baselmmdecoder.h>

#include <QList>
#include <QHash>
#include <QMutex>
#include <QSemaphore>

class CircularBuffer;
class RawBuffer;
class BaseLmmElement;
struct decodeTimeStamp;
struct _Buffer_Object;
struct Engine_Obj;
struct Vdec2_Object;
struct BufTab_Object;
typedef struct _Buffer_Object *Buffer_Handle;
typedef struct Engine_Obj *Engine_Handle;
typedef struct Vdec2_Object *Vdec2_Handle;
typedef struct BufTab_Object *BufTab_Handle;

class DmaiDecoder : public BaseLmmDecoder
{
	Q_OBJECT
public:
	enum bufferUseFlags {
		CODEC_USE	= 0x1,
		OUTPUT_USE	= 0x2
	};
	enum codecType {
		MPEG2,
		MPEG4
	};
	explicit DmaiDecoder(codecType c,QObject *parent = 0);
	~DmaiDecoder();

	int flush();

	void aboutToDeleteBuffer(const RawBufferParameters *params);

	static void initCodecEngine();
	static void cleanUpDsp();
protected:
	int decode(RawBuffer bufsrc);
public slots:
private:
	Engine_Handle hEngine;
	Vdec2_Handle hCodec;
	BufTab_Handle hBufTab;
	int numOutputBufs;
	CircularBuffer *circBuf; /* TODO: This is no longer needed, convert to RawBuffer */
	Buffer_Handle circBufData;
	int decodeCount;
	codecType codec;
	QHash<int, RawBuffer> bufferMapping;
	QSemaphore dispWaitSem;
	QSemaphore decodeWaitCounter;
	QMutex buftabLock;

	int startCodec();
	int stopCodec();
	int startDecoding();
	int stopDecoding();
	void releaseFreeBuffers();
};

#endif // DMAIDECODER_H
