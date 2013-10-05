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
		duration = 0;
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
		mimeType = other.mimeType;
	}

	virtual ~RawBufferData();

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
	QHash<QString, QVariant> parameters;
	int bufferNo;
	BaseLmmElement* myParent;
	QString mimeType;
};

class RawBuffer
{
public:
	explicit RawBuffer(BaseLmmElement *parent = 0);
	explicit RawBuffer(QString mimeType, const void *data, int size, BaseLmmElement *parent = 0);
	explicit RawBuffer(QString mimeType, int size, BaseLmmElement *parent = 0);
	RawBuffer(const RawBuffer &other);
	virtual ~RawBuffer();
	static RawBuffer eof(BaseLmmElement *parent = 0);

	void setParentElement(BaseLmmElement *el);
	void setRefData(QString mimeType, void *data, int size);
	void addBufferParameter(QString, QVariant);
	void addBufferParameters(const QHash<QString, QVariant> &other);
	const QHash<QString, QVariant> bufferParameters() const;
	QVariant getBufferParameter(QString) const;
	void setSize(int size);
	int prepend(const void *data, int size);
	const void * constData() const;
	void * data();
	int size() const;
	int setUsedSize(int size);
	void setDuration(unsigned int val);
	unsigned int getDuration() const;
	void setPts(qint64 val);
	qint64 getPts() const;
	void setDts(qint64 val);
	qint64 getDts() const;
	QString getMimeType() const;

	void setStreamBufferNo(int val);
	int streamBufferNo() const;
	friend class RawBufferData;

	virtual bool operator==(const RawBuffer& other);
signals:
	
public slots:
protected:
	QExplicitlySharedDataPointer<RawBufferData> d;
private:
};

#endif // RAWBUFFER_H
