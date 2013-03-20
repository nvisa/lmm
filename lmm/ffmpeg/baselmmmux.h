#ifndef BASELMMMUX_H
#define BASELMMMUX_H

#include <lmm/baselmmelement.h>

#include <stdint.h>

struct AVPacket;
struct AVFormatContext;
struct AVStream;
struct AVOutputFormat;
struct AVInputFormat;
struct URLContext;

class BaseLmmMux : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmMux(QObject *parent = 0);

	virtual int start();
	virtual int stop();
	virtual int sync();
	virtual int muxNextBlocking();

	/* ffmpeg url routines */
	int readPacket(uint8_t *buffer, int buf_size);
	int writePacket(const uint8_t *buffer, int buf_size);
	int openUrl(QString url, int flags);
	int closeUrl(URLContext *h);
signals:
	void inputInfoFound();
public slots:
protected:
	int libavAnalayzeDuration;
	QString sourceUrlName;
	AVFormatContext *context;
	AVFormatContext *inputContext;
	AVOutputFormat *fmt;
	AVInputFormat *inputFmt;
	bool foundStreamInfo;
	int muxedBufferCount;

	int videoStreamIndex;
	int audioStreamIndex;
	AVStream *audioStream;
	AVStream *videoStream;

	QList<RawBuffer> inputInfoBuffers;

	int muxNumber; /* global mux number of this instance, needed for UrlProtocol */
	bool muxOutputOpened;

	virtual int initMuxer();
	virtual int findInputStreamInfo();
	virtual QString mimeType() = 0;
	void printInputInfo();
	void muxNext();
	virtual qint64 packetTimestamp();
};

#endif // BASELMMMUX_H