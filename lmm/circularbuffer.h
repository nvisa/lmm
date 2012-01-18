#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#include <QObject>

class CircularBuffer : public QObject
{
	Q_OBJECT
public:
	explicit CircularBuffer(QObject *parent = 0);
	explicit CircularBuffer(void *source, int size, QObject *parent = 0);

	void * getDataPointer();
	int usedSize();
	int useData(int size);
	int addData(const void *data, int size);
	int reset();
signals:
	
public slots:
private:
	char *head;
	char *tail;
	char *rawData;
	int rawDataLen;
	int freeBufLen;
	int usedBufLen;
};

#endif // CIRCULARBUFFER_H
