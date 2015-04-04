#ifndef DM365DMACOPY_H
#define DM365DMACOPY_H

#include <lmm/baselmmelement.h>
#include <lmm/dmai/dmaibuffer.h>

#include <QMutex>

class DM365DmaCopy : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit DM365DmaCopy(QObject *parent = 0);
	int dmaCopy(void *src, void *dst, int acnt, int bcnt);
	void setBufferCount(int cnt) { bufferCount = cnt; }

protected:
	void aboutToDeleteBuffer(const RawBufferParameters *pars);
	int processBuffer(const RawBuffer &buf);
	RawBuffer createAndCopy(const RawBuffer &buf);

	int mmapfd;
	QMutex dmalock;
	QMutex buflock;
	int bufferCount;
	QList<DmaiBuffer> pool;
	QList<DmaiBuffer> freeBuffers;
};

#endif // DM365DMACOPY_H
