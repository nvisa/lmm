#ifndef RAWBUFFER_H
#define RAWBUFFER_H

#include <QObject>
#include <QMap>
#include <QSharedData>
#include <QExplicitlySharedDataPointer>
#include <QVariant>

#include <stdlib.h>

class BaseLmmElement;
class RawBufferData;
class RawBuffer;

class RawBufferParameters {
public:
	RawBufferParameters();
	RawBufferParameters(const RawBufferParameters &other);
	quintptr *avFrame;
	quintptr *avPacket;
	int avPixelFormat;
	uint bufferNo;
	qint64 captureTime;
	quintptr *dmaiBuffer;
	uint duration;
	qint64 encodeTime;
	int fps;
	int frameType;
	int h264NalType;
	quintptr * omxBuf;
	int poolIndex;
	int pts;
	int streamBufferNo;
	quintptr *targetElement;
	quintptr *v4l2Buffer;
	int v4l2PixelFormat;
	int videoWidth;
	int videoHeight;
	QByteArray metaData;
};

class RawBufferData : public QSharedData
{
public:
	RawBufferData()
	{
		uuid = rand();
		rawData = NULL;
		refData = false;
		usedLen = 0;
		myParent = NULL;
		parameters = new RawBufferParameters;
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
		parameters = other.parameters;
		myParent = other.myParent;
		mimeType = other.mimeType;
		uuid = other.uuid;
	}

	virtual ~RawBufferData();

	bool refData;
	char *rawData;
	int usedLen;
	int rawDataLen;
	int prependPos;
	int prependLen;
	int appendLen;
	int uuid;
	BaseLmmElement* myParent;
	QString mimeType;
	RawBufferParameters *parameters;
};

class RawBuffer
{
public:
	explicit RawBuffer(BaseLmmElement *parent = 0);
	explicit RawBuffer(QString mimeType, const void *data, int size, BaseLmmElement *parent = 0);
	explicit RawBuffer(QString mimeType, int size, BaseLmmElement *parent = 0);
	RawBuffer(const RawBuffer &other);
	virtual ~RawBuffer();
	RawBuffer makeCopy(bool noPointers = true) const;
	int setParameters(const RawBufferParameters *pars, bool noPointers = true);

	void setParentElement(BaseLmmElement *el);
	void setRefData(QString mimeType, void *data, int size);
	void setSize(int size);
	int prepend(const void *data, int size);
	const void * constData() const;
	void * data();
	int size() const;
	int setUsedSize(int size);
	static QByteArray serializeMetadata(const QHash<QString, QVariant> &hash);
	static QHash<QString, QVariant> deserializeMetadata(const QByteArray &ba);
	QString getMimeType() const;
	RawBufferParameters * pars();
	const RawBufferParameters * constPars() const;

	friend class RawBufferData;

	virtual bool operator==(const RawBuffer& other);

	qint32 getUniqueId() const;
signals:
	
public slots:
protected:
	QExplicitlySharedDataPointer<RawBufferData> d;
private:
};

#endif // RAWBUFFER_H
