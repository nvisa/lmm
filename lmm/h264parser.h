#ifndef SIMPLEH264PARSER_H
#define SIMPLEH264PARSER_H

#include "baselmmparser.h"

class H264SeiInfo {
public:
	H264SeiInfo()
	{
		size = 0;
		offset = -1;
		type = -1;
	}

	int size;
	int offset;
	int type;
	QByteArray payload;
};

class SimpleH264Parser : public BaseLmmParser
{
	Q_OBJECT
public:
	explicit SimpleH264Parser(QObject *parent = 0);
	static const uchar * findNextStartCode(const uchar *p, const uchar *end);
	static int getNalType(const uchar *data);
	void setSpsPpsInsertion(bool v) { insertSpsPps = v; }
	void setInputPacketized(bool v) { inputPacketized = v; }
	void setExtractSei();
	H264SeiInfo getSeiData(int bufferNo);
signals:
	
public slots:
protected:
	enum h264_output {
		H264_OUTPUT_NALU,
		H264_OUTPUT_AU,
	};
	int findNextStartCode(const uchar *data, int size);
	int parse(const uchar *data, int size);
	H264SeiInfo parseNoStart(const uchar *data);
	int processBuffer(const RawBuffer &buf);
	void extractSeiData(RawBuffer buf);

	int packState;
	int packSize;
	h264_output h264Mode;
	QList<RawBuffer> nalBuffers;
	QList<RawBuffer> au;
	RawBuffer idrBuf;
	RawBuffer spsBuf;
	RawBuffer ppsBuf;
	bool insertSpsPps;
	bool inputPacketized;
	bool extractSei;
	QMap<int, H264SeiInfo> seiData;
};

#endif // SIMPLEH264PARSER_H
