#ifndef METADATAGENERATOR_H
#define METADATAGENERATOR_H

#include <lmm/baselmmelement.h>

class MetadataGenerator : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit MetadataGenerator(QObject *parent = 0);

	virtual int processBlocking(int ch = 0);

protected:
	virtual int processBuffer(const RawBuffer &buf);

	QString macAddr;
	QString appVersion;
	QString superVersion;
	QString fwVersion;
	QString hwVersion;
	QString serialNo;
	QString streamerVersion;
};

#endif // METADATAGENERATOR_H
