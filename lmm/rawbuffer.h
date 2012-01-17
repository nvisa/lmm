#ifndef RAWBUFFER_H
#define RAWBUFFER_H

#include <QObject>
#include <QMap>

class RawBuffer : public QObject
{
	Q_OBJECT
public:
	explicit RawBuffer(QObject *parent = 0);
	explicit RawBuffer(void *data, int size, QObject *parent = 0);
	~RawBuffer();

	void setRefData(void *data, int size);
	void addBufferParameter(QString, QVariant);
	QVariant getBufferParameter(QString);
	void setSize(int size);
	int prepend(const void *data, int size);
	const void * constData();
	void * data();
	int size() { return rawDataLen; }
	void setDuration(unsigned int val) { duration = val; }
	unsigned int getDuration() { return duration; }
	void setPts(qint64 val) { pts = val; }
	qint64 getPts() { return pts; }

	void setStreamBufferNo(int val) { bufferNo = val; }
	int streamBufferNo() { return bufferNo; }
signals:
	
public slots:
private:
	bool refData;
	char *rawData;
	int rawDataLen;
	int prependPos;
	int prependLen;
	int appendLen;
	unsigned int duration;
	qint64 pts;
	QMap<QString, QVariant> parameters;
	int bufferNo;
};

#endif // RAWBUFFER_H
