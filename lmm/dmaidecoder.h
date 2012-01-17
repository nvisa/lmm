#ifndef DMAIDECODER_H
#define DMAIDECODER_H

#include <QObject>
#include <QList>

#ifdef __cplusplus
extern "C" {
#endif
#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Vdec2.h>
#include <ti/sdo/dmai/Time.h>
#ifdef __cplusplus
}
#endif

class CircularBuffer;
class RawBuffer;

class DmaiDecoder : public QObject
{
	Q_OBJECT
public:
	explicit DmaiDecoder(QObject *parent = 0);
	~DmaiDecoder();

	int start();
	int stop();
	int addBuffer(RawBuffer *);
	int decodeOne();
	RawBuffer * nextBuffer();

	static void initCodecEngine();
	static void cleanUpDsp();
signals:
	
public slots:
private:
	Engine_Handle hEngine;
	Vdec2_Handle hCodec;
	BufTab_Handle hBufTab;
	int numOutputBufs;
	QList<RawBuffer *> inputBuffers;
	QList<RawBuffer *> outputBuffers;
	CircularBuffer *circBuf;
	Buffer_Handle circBufData;

	int startCodec();
	int stopCodec();
};

#endif // DMAIDECODER_H
