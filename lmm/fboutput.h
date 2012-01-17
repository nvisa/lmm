#ifndef FBOUTPUT_H
#define FBOUTPUT_H

#include <QObject>
#include <QList>

class RawBuffer;
class QTime;

class FbOutput : public QObject
{
	Q_OBJECT
public:
	explicit FbOutput(QObject *parent = 0);
	int addBuffer(RawBuffer *buffer);
	int output();
	void setStreamTime(QTime *t) { streamTime = t; }
	void setStreamDuration(qint64 duration) { streamDuration = duration; }
signals:
	
public slots:
private:
	int fd;
	int fbSize;
	unsigned char *fbAddr;
	int fbLineLen;
	int fbHeight;
	QList<RawBuffer *> buffers;
	QTime *streamTime;
	qint64 streamDuration;

	int openFb(QString filename);
};

#endif // FBOUTPUT_H
