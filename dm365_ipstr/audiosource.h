#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include <lmm/baselmmelement.h>

class AudioSource : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit AudioSource(QObject *parent = 0);

	virtual int processBlocking(int ch = 0);

protected:

	virtual int processBuffer(const RawBuffer &buf);

};

#endif // AUDIOSOURCE_H
