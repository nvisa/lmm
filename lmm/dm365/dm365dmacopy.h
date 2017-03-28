#ifndef DM365DMACOPY_H
#define DM365DMACOPY_H

#include <lmm/baselmmelement.h>
#include <lmm/dmai/dmaibuffer.h>

#include <QRect>
#include <QMutex>

class DM365DmaCopy : public BaseLmmElement
{
	Q_OBJECT
public:

	enum operationMode {
		MODE_BUFFER_COPY,
		MODE_OVERLAY
	};

	/*struct OverlayDef {
		int x;
		int y;
		int w;
		int h;
	};*/

	explicit DM365DmaCopy(QObject *parent = 0, int outCnt = 2);
	int dmaCopy(void *src, void *dst, int acnt, int bcnt);
	void setBufferCount(int cnt) { bufferCount = cnt; }
	void setAllocateSize(int size);

	void addOverlay(int x, int y, int w, int h);
	void setOverlay(int index, int x, int y, int w, int h);
	void setOverlay(int index, const QRect &r);
	QRect getOverlay(int index);

protected:
	void aboutToDeleteBuffer(const RawBufferParameters *pars);
	int processBuffer(const RawBuffer &buf);
	int processBufferOverlay(const RawBuffer &buf);
	RawBuffer createAndCopy(const RawBuffer &buf);

	int mmapfd;
	int outputCount;
	int allocSize;
	QMutex dmalock;
	QMutex buflock;
	QMutex olock;
	int bufferCount;
	QList<DmaiBuffer> pool;
	QList<DmaiBuffer> freeBuffers;
	operationMode mode;
	QList<QRect> overlays;
};

#endif // DM365DMACOPY_H
