#ifndef MULTIBUFFERSOURCE_H
#define MULTIBUFFERSOURCE_H

#include <lmm/baselmmelement.h>

class MultiBufferSourcePriv;

class MultiBufferSource : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit MultiBufferSource(QObject *parent = 0);

protected:
	virtual int processBuffer(const RawBuffer &buf);
	virtual int processBuffer(int ch, const RawBuffer &buf);

private:
	void checkPool(int bufferNo);

	MultiBufferSourcePriv *priv;
};

#endif // MULTIBUFFERSOURCE_H
