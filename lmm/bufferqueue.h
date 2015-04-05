#ifndef BUFFERQUEUE_H
#define BUFFERQUEUE_H

#include <lmm/rawbuffer.h>
#include <lmm/baselmmelement.h>

class BufferQueue : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BufferQueue(QObject *parent = 0);
	RawBuffer getBuffer(int ind);
	RawBuffer findBuffer(qint64 ts);
	RawBuffer getLast();
	void setQueueSize(int length);
	int getBufferCount();
protected:
	int processBuffer(const RawBuffer &buf);

	QList<RawBuffer> buffers;
	int queueLen;
	QMutex block;
};

#endif // BUFFERQUEUE_H
