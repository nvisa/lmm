#ifndef RAWBUFFER_H
#define RAWBUFFER_H

#include <QObject>
#include <QMap>

class BaseLmmElement;

class RawBuffer : public QObject
{
	Q_OBJECT
public:
	explicit RawBuffer(BaseLmmElement *parent = 0);
	explicit RawBuffer(void *data, int size, BaseLmmElement *parent = 0);
	explicit RawBuffer(int size, BaseLmmElement *parent = 0);
	~RawBuffer();

	void setParentElement(BaseLmmElement *el) { myParent = el; }
	void setRefData(void *data, int size);
	void addBufferParameter(QString, QVariant);
	QVariant getBufferParameter(QString);
	void setSize(int size);
	int prepend(const void *data, int size);
	const void * constData();
	void * data();
	int size() { return usedLen; }
	int setUsedSize(int size);
	void setDuration(unsigned int val) { duration = val; }
	unsigned int getDuration() { return duration; }
	void setPts(qint64 val) { pts = val; }
	qint64 getPts() { return pts; }
	void setDts(qint64 val) { dts = val; }
	qint64 getDts() { return dts; }

	void setStreamBufferNo(int val) { bufferNo = val; }
	int streamBufferNo() { return bufferNo; }
signals:
	
public slots:
private:
	bool refData;
	char *rawData;
	int usedLen;
	int rawDataLen;
	int prependPos;
	int prependLen;
	int appendLen;
	unsigned int duration;
	qint64 pts;
	qint64 dts;
	QMap<QString, QVariant> parameters;
	int bufferNo;
	BaseLmmElement* myParent;
};

#endif // RAWBUFFER_H
