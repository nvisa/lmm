#ifndef METADATAGENERATOR_H
#define METADATAGENERATOR_H

#include <lmm/baselmmelement.h>

#include <QMutex>

class MetadataGenerator : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit MetadataGenerator(QObject *parent = 0);

	virtual int processBlocking(int ch = 0);

	int setCustomData(qint32 id, const QByteArray &ba, int count = -1);
	int removeCustomData(qint32 id);

protected:
	virtual int processBuffer(const RawBuffer &buf);

	QString macAddr;
	QString appVersion;
	QString superVersion;
	QString fwVersion;
	QString hwVersion;
	QString serialNo;
	QString streamerVersion;

	struct DataOverlay {
		QByteArray data;
		int count;
		int id;
	};
	QHash<qint32, DataOverlay *> overlays;
	QMutex lock;
};

#endif // METADATAGENERATOR_H
