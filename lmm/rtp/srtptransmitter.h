#ifndef SRTPTRANSMITTER_H
#define SRTPTRANSMITTER_H

#include <lmm/rtp/rtptransmitter.h>

class SRtpChannelPriv;

class SRtpChannel : public RtpChannel
{
	Q_OBJECT
public:
	SRtpChannel(int psize, const QHostAddress &myIpAddr);

	SRtpChannelPriv *p;
protected:
	virtual void writeSockData(const char *buf, int size);

};

class SRtpTransmitter : public RtpTransmitter
{
	Q_OBJECT
public:
	explicit SRtpTransmitter(QObject *parent = 0);

	virtual RtpChannel * addChannel();
	virtual int setupChannel(RtpChannel *ch, const QString &target, int dport, int dcport, int sport, int scport, uint ssrc);

protected:
	virtual RtpChannel * createChannel();
};

#endif // SRTPTRANSMITTER_H
