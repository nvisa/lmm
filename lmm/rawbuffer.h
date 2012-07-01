#ifndef RAWBUFFER_H
#define RAWBUFFER_H

#include <QObject>
#include <QMap>
#include <QSharedData>
#include <QExplicitlySharedDataPointer>
#include <QVariant>

class BaseLmmElement;
class RawBufferData;
class RawBuffer;

class RawBufferData : public QSharedData
{
public:
	RawBufferData()
	{
		rawData = NULL;
		refData = false;
		usedLen = 0;
		bufferNo = 0;
		myParent = NULL;
	}

	RawBufferData(const RawBufferData &other)
		: QSharedData(other)
	{
		refData = other.refData;
		rawData = other.rawData;
		usedLen = other.usedLen;
		rawDataLen = other.rawDataLen;
		prependPos = other.prependPos;
		prependLen = other.prependLen;
		appendLen = other.appendLen;
		duration = other.duration;
		pts = other.pts;
		dts = other.dts;
		parameters = other.parameters;
		bufferNo = other.bufferNo;
		myParent = other.myParent;
	}

	~RawBufferData();

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

class RawBuffer
{
public:
	explicit RawBuffer(BaseLmmElement *parent = 0);
	explicit RawBuffer(void *data, int size, BaseLmmElement *parent = 0);
	explicit RawBuffer(int size, BaseLmmElement *parent = 0);
	RawBuffer(const RawBuffer &other);
	~RawBuffer();

	void setParentElement(BaseLmmElement *el);
	void setRefData(void *data, int size);
	void addBufferParameter(QString, QVariant);
	QVariant getBufferParameter(QString);
	void setSize(int size);
	int prepend(const void *data, int size);
	const void * constData() const;
	void * data();
	int size();
	int setUsedSize(int size);
	void setDuration(unsigned int val);
	unsigned int getDuration();
	void setPts(qint64 val);
	qint64 getPts();
	void setDts(qint64 val);
	qint64 getDts();

	void setStreamBufferNo(int val);
	int streamBufferNo();
	friend class RawBufferData;
signals:
	
public slots:
private:
	QExplicitlySharedDataPointer<RawBufferData> d;
};

#endif // RAWBUFFER_H
