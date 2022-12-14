#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#include <QObject>

class QMutex;
class QSemaphore;

class CircularBuffer : public QObject
{
	Q_OBJECT
public:
	explicit CircularBuffer(QObject *parent = 0);
	explicit CircularBuffer(void *source, int size, QObject *parent = 0);
	explicit CircularBuffer(int size, QObject *parent = 0);
	~CircularBuffer();

	void * getDataPointer();
	int usedSize();
	int freeSize();
	int totalSize();
	int useData(int size);
	int addData(const void *data, int size);
	int addDataNoShift(const void *data, int size);
	int reset();
	int wait(int size);
	int waitWr(int size);

	void lock();
	void unlock();
signals:
	
public slots:
private:
	bool dataOwner;
	char *head;
	char *tail;
	char *rawData;
	int rawDataLen;
	int freeBufLen;
	int usedBufLen;

	QMutex *mutex;
	QSemaphore *waitSem;
	QSemaphore *waitSemWr;
	Qt::HANDLE lockThread;
};

#endif // CIRCULARBUFFER_H
