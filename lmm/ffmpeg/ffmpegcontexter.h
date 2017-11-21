#ifndef FFMPEGCONTEXTER_H
#define FFMPEGCONTEXTER_H

#include <lmm/baselmmelement.h>

class FFmpegContexterPriv;

class FFmpegContexter : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit FFmpegContexter(QObject *parent = 0);

	virtual int start();
protected:
	int processBuffer(int ch, const RawBuffer &buf);
private:
	friend class FFmpegContexterPriv;
	static int read_packet(void *opaque, uchar *buf, int size);
	int readPacket(uchar *buffer, int size);

	QMutex buflock;
	QList<RawBuffer> inputBuffers;

};
#endif // FFMPEGCONTEXTER_H
