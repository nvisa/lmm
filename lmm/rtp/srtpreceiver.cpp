#include "srtpreceiver.h"
#include "debug.h"

#include <srtp/srtp.h>

class SRtpReceiverPriv
{
public:
	SRtpReceiverPriv()
	{
		memset(&policy, 0x0, sizeof(srtp_policy_t));
		sbuf = new char[2048];
	}

	srtp_t session;
	srtp_policy_t policy;
	char *sbuf;
};

SRtpReceiver::SRtpReceiver(QObject *parent)
	: RtpReceiver(parent)
{
	p = NULL;

	static int initlib = 0;
	if (!initlib) {
		srtp_init();
		initlib = 1;
	}
}

int SRtpReceiver::processRtpData(const QByteArray &ba, const QHostAddress &sender, quint16 senderPort)
{
	if (!p) {
		p = new SRtpReceiverPriv;

		// Set key to predetermined value
		static uint8_t key[30] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
						   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
						   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
						   0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D};
		// set policy to describe a policy for an SRTP stream
		crypto_policy_set_rtp_default(&p->policy.rtp);
		crypto_policy_set_rtcp_default(&p->policy.rtcp);
		p->policy.ssrc.type = ssrc_specific;
		p->policy.ssrc.value = 0x43123423;
		p->policy.key  = key;
		p->policy.next = NULL;

		//p->policy.rtp.cipher_type = AES_ICM;
		//p->policy.rtp.auth_type = HMAC_SHA1;

		srtp_create(&p->session, &p->policy);
	}

	int size = ba.size();
	memcpy(p->sbuf, ba.constData(), ba.size());
	int err = srtp_unprotect(p->session, p->sbuf, &size);
	if (err) {
		mDebug("error '%d' decoding SRTP packet", err);
		return 0;
	}
	QByteArray dec(p->sbuf, size);
	return RtpReceiver::processRtpData(dec, sender, senderPort);
}

