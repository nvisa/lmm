#ifndef ALSAINPUT_H
#define ALSAINPUT_H

#include <lmm/lmmcommon.h>
#include <lmm/baselmmelement.h>

class Alsa;
class AlsaInput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit AlsaInput(Lmm::CodecType codec, QObject *parent = 0);

	virtual int start();
	virtual int stop();
	virtual int processBlocking(int ch = 0);
	virtual int processBuffer(const RawBuffer &buf);
signals:
	
public slots:
protected:
	Alsa *alsaIn;
	int bufferSize;
	Lmm::CodecType outputFormat;
};

#endif // ALSAINPUT_H
