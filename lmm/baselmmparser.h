#ifndef BASELMMPARSER_H
#define BASELMMPARSER_H

#include <lmm/baselmmelement.h>

class BaseLmmParser : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmParser(QObject *parent = 0);
signals:
	
public slots:
protected:
	virtual int processBuffer(const RawBuffer &buf);
};

#endif // BASELMMPARSER_H
