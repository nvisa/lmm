#ifndef BASELMMPARSER_H
#define BASELMMPARSER_H

#include <lmm/baselmmelement.h>

class CircularBuffer;

class BaseLmmParser : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmParser(QObject *parent = 0);
	int parseBlocking();
signals:
	
public slots:
protected:
	CircularBuffer *circBuf;
	QMutex shiftLock;

	virtual int processBuffer(RawBuffer buf);
	virtual int parse(const uchar *data, int size) = 0;
};

#endif // BASELMMPARSER_H
