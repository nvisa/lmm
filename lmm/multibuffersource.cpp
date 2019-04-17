#include "multibuffersource.h"
#include "debug.h"
#include "rawbuffer.h"

#include <QHash>

class MultiBufferSourcePriv
{
public:
	QMutex poolLock;
	QHash<int, QHash<int, RawBuffer > > pool;
};

MultiBufferSource::MultiBufferSource(QObject *parent)
	: BaseLmmElement(parent)
{
	priv = new MultiBufferSourcePriv;
}

int MultiBufferSource::processBuffer(const RawBuffer &buf)
{
	/* remember, this is channel 0 */
	QMutexLocker ml(&priv->poolLock);
	priv->pool[buf.constPars()->streamBufferNo][0] = buf;
	checkPool(buf.constPars()->streamBufferNo);
	return newOutputBuffer(0, buf);;
}

int MultiBufferSource::processBuffer(int ch, const RawBuffer &buf)
{
	QMutexLocker ml(&priv->poolLock);
	priv->pool[buf.constPars()->streamBufferNo][ch] = buf;
	checkPool(buf.constPars()->streamBufferNo);
	return newOutputBuffer(ch, buf);
}

void MultiBufferSource::checkPool(int bufferNo)
{
	const QHash<int, RawBuffer > &bufs = priv->pool[bufferNo];
	if (bufs.size() == getInputQueueCount()) {
		/* release a new buffer */
		RawBuffer outbuf("application/multi-buffer", 1);
		for (int i = 0; i < getInputQueueCount(); i++)
			outbuf.pars()->subbufs << bufs[i];
		newOutputBuffer(getInputQueueCount(), outbuf);
		priv->pool.remove(bufferNo);
	}
}

