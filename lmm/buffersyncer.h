#ifndef BUFFERSYNCER_H
#define BUFFERSYNCER_H

#include <lmm/baselmmelement.h>

class BufferSyncThread;
class BaseLmmOutput;

class BufferSyncer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BufferSyncer(int threadCount = 20, QObject *parent = 0);
	void setSyncEnabled(bool en) { sync = en; }
	bool syncEnabled() { return sync; }
signals:
	
public slots:
protected:
	bool sync;
	QMutex mutex;
	QList<BufferSyncThread *> threads;
	BufferSyncThread * findEmptyThread();
	int processBuffer(const RawBuffer &buffer);
};

#endif // BUFFERSYNCER_H
