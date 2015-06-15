#ifndef RTPDEPACKETIZER_H
#define RTPDEPACKETIZER_H

#include <lmm/baselmmelement.h>

#include <QMutex>
#include <QHostAddress>

class QUdpSocket;

class RtpDePacketizer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit RtpDePacketizer(QObject *parent = 0);

	int start();
	int stop();

	int getSourceDataPort() { return srcDataPort; }
	int getSourceControlPort() { return srcControlPort; }
	int getDestinationDataPort() { return dstDataPort; }
	int getDestinationControlPort() { return dstControlPort; }
	void setDestinationDataPort(int port) { dstDataPort = port; }
	void setDestinationControlPort(int port) { dstControlPort = port; }
	void setSourceDataPort(int port) { srcDataPort = port; }
	void setSourceControlPort(int port) { srcControlPort = port; }

protected slots:
	void readPendingRtpDatagrams();

protected:
	virtual int processBuffer(const RawBuffer &buf);
	int processRtpData(const QByteArray &ba);

	QUdpSocket *sock;
	QHostAddress myIpAddr;
	QMutex streamLock;

	int dstDataPort;
	int dstControlPort;
	int srcDataPort;
	int srcControlPort;

	uint seqLast;
	QByteArray currentNal;
	QByteArray annexPrefix;
	int maxPayloadSize;
};

#endif // RTPDEPACKETIZER_H
