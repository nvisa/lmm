#include "srtptransmitter.h"

#include <srtp/srtp.h>

#include <QUdpSocket>

class SRtpChannelPriv
{
public:
	SRtpChannelPriv()
	{
		memset(&policy, 0x0, sizeof(srtp_policy_t));
		sbuf = new char[2048];
	}

	srtp_t session;
	srtp_policy_t policy;
	char *sbuf;
};

SRtpTransmitter::SRtpTransmitter(QObject *parent)
	: RtpTransmitter(parent)
{
	static int initlib = 0;
	if (!initlib) {
		srtp_init();
		initlib = 1;
	}
}

RtpChannel *SRtpTransmitter::addChannel()
{
	return RtpTransmitter::addChannel();
}

int SRtpTransmitter::setupChannel(RtpChannel *ch, const QString &target, int dport, int dcport, int sport, int scport, uint ssrc)
{
	SRtpChannel *sch = (SRtpChannel *)ch;

	// Set key to predetermined value
	static uint8_t key[30] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
					   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
					   0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D};
	// set policy to describe a policy for an SRTP stream
	crypto_policy_set_rtp_default(&sch->p->policy.rtp);
	crypto_policy_set_rtcp_default(&sch->p->policy.rtcp);
	sch->p->policy.ssrc.type = ssrc_specific;
	sch->p->policy.ssrc.value = ssrc;
	sch->p->policy.key  = key;
	sch->p->policy.next = NULL;

	srtp_create(&sch->p->session, &sch->p->policy);

	return RtpTransmitter::setupChannel(ch, target, dport, dcport, sport, scport, ssrc);
}

RtpChannel *SRtpTransmitter::createChannel()
{
	return new SRtpChannel(maxPayloadSize, myIpAddr);
}

SRtpChannel::SRtpChannel(int psize, const QHostAddress &myIpAddr)
	: RtpChannel(psize, myIpAddr)
{
	p = new SRtpChannelPriv;
}

void SRtpChannel::writeSockData(const char *buf, int size)
{
	memcpy(p->sbuf, buf, size);
	int ssize = size;
	int err = srtp_protect(p->session, (void *)p->sbuf, &ssize);
	if (err)
		qDebug() << "write error" << err;
	sock->writeDatagram(p->sbuf, ssize, QHostAddress(dstIp), dstDataPort);
}
