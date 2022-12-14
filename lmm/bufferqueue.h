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
	QList<RawBuffer> findBuffers(qint64 ts, int count);
	RawBuffer getLast();
	QList<RawBuffer> getLast(int count);
	void setQueueSize(int length);
	int getBufferCount();
	int getTotalSize() { return totalSize; }
protected:
	int processBuffer(const RawBuffer &buf);
	int processBlocking(int ch);

	QList<RawBuffer> buffers;
	int queueLen;
	QMutex block;
	int totalSize;

	Qt::HANDLE processingThread;
};

#endif // BUFFERQUEUE_H
