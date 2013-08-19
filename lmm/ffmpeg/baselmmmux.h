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
	virtual int setOutputFilename(QString filename);

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
	int avioBufferSizeIn;
	int avioBufferSizeOut;
	uchar *avioBufferIn;
	uchar *avioBufferOut;
	/*
	 * In older FFmpeg releases AVIOContext is typedef to
	 * anonymous struct so it is not possible to forward
	 * declare here. This behaviour is fixed in later releases
	 * but we define this as 'void *' to be compatiple with
	 * old releases.
	 */
	void *avioCtxIn;
	void *avioCtxOut;
	QMutex mutex;

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
	int processBuffer(RawBuffer buf);
	virtual qint64 packetTimestamp();
	virtual int timebaseNum();
	virtual int timebaseDenom();
};

#endif // BASELMMMUX_H
