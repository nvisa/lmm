#ifndef BUFFERSYNCER_H
#define BUFFERSYNCER_H

#include <lmm/baselmmelement.h>

class BufferSyncThread;
class BaseLmmOutput;

class BufferSyncer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BufferSyncer(QObject *parent = 0);
	int addBuffer(BaseLmmOutput *target, RawBuffer buffer);
signals:
	
public slots:
protected:
	QList<BufferSyncThread *> threads;
};

#endif // BUFFERSYNCER_H
