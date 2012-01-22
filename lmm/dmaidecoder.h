#ifndef DMAIDECODER_H
#define DMAIDECODER_H

#include "baselmmdecoder.h"
#include <QList>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Vdec2.h>
#include <ti/sdo/dmai/Time.h>

class CircularBuffer;
class RawBuffer;
class BaseLmmElement;
struct decodeTimeStamp;

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

	int decode() { return decodeOne(); }
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
	codecType codec;

	int startCodec();
	int stopCodec();
	int startDecoding();
	int stopDecoding();
};

#endif // DMAIDECODER_H
