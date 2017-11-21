#ifndef SRTPRECEIVER_H
#define SRTPRECEIVER_H

#include <lmm/rtp/rtpreceiver.h>

class SRtpReceiverPriv;

class SRtpReceiver : public RtpReceiver
{
	Q_OBJECT
public:
	explicit SRtpReceiver(QObject *parent = 0);

signals:

public slots:
protected:
	virtual int processRtpData(const QByteArray &ba, const QHostAddress &sender, quint16 senderPort);

	SRtpReceiverPriv *p;
};

#endif // SRTPRECEIVER_H
