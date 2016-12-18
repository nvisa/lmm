#ifndef IMAGESNAPSHOTINTERFACE_H
#define IMAGESNAPSHOTINTERFACE_H

#include <lmm/lmmcommon.h>
#include <lmm/rawbuffer.h>

#include <QList>

class ImageSnapshotInterface
{
public:
	virtual QList<RawBuffer> getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount) = 0;
};

#endif // IMAGESNAPSHOTINTERFACE_H

