#ifndef DMAIDECODER_H
#define DMAIDECODER_H

#include "baselmmdecoder.h"
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
class BaseLmmElement;
struct decodeTimeStamp;

class DmaiDecoder : public BaseLmmDecoder
{
	Q_OBJECT
public:
	explicit DmaiDecoder(QObject *parent = 0);
	~DmaiDecoder();

	int decodeOne();
	int flush();

	static void initCodecEngine();
	static void cleanUpDsp();
signals:
	
public slots:
private:
	Engine_Handle hEngine;
	Vdec2_Handle hCodec;
	BufTab_Handle hBufTab;
	int numOutputBufs;
	CircularBuffer *circBuf;
	Buffer_Handle circBufData;
	int decodeCount;

	int startCodec();
	int stopCodec();
	int startDecoding();
	int stopDecoding();
};

#endif // DMAIDECODER_H
