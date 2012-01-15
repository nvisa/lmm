#ifndef RAWBUFFER_H
#define RAWBUFFER_H

#include <QObject>

class RawBuffer : public QObject
{
	Q_OBJECT
public:
	explicit RawBuffer(QObject *parent = 0);
	explicit RawBuffer(void *data, int size, QObject *parent = 0);
	~RawBuffer();

	void setSize(int size);
	int prepend(const void *data, int size);
	const void * constData();
	void * data();
	int size() { return rawDataLen; }
	void setDuration(unsigned int val) { duration = val; }
	unsigned int getDuration() { return duration; }
signals:
	
public slots:
private:
	char *rawData;
	int rawDataLen;
	int prependPos;
	int prependLen;
	int appendLen;
	unsigned int duration;
};

#endif // RAWBUFFER_H
