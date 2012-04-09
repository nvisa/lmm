#ifndef UDPOUTPUT_H
#define UDPOUTPUT_H

#include <baselmmoutput.h>

#include <QHostAddress>

class QUdpSocket;

#define HEADER_SIZE 8
#define DATAGRAM_SIZE (500 - HEADER_SIZE)

class UdpOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	enum PacketType {
		PT_SES,
		PT_EES,
		PT_ESB,
		PT_HES,
		PT_CP,
	};
	enum CommandType {
		CT_START,
		CT_STOP,
		CT_FLUSH,
	};
	enum ClientStatus {
		CS_UNKNOWN,
		CS_CONNECTED,
		CS_STARTED,
		CS_STOPPED,
	};
	explicit UdpOutput(QObject *parent = 0);
	virtual int start();
	virtual int stop();

	static QByteArray createControlCommand(CommandType cmd,
										   QVariant var = QVariant());
private slots:
	void dataReady();
private:
	virtual int outputBuffer(RawBuffer buf);
	void processTheDatagram(const QByteArray &);
	void writeData(const QByteArray &data);

	QUdpSocket *sock;
	ClientStatus clientStatus;
	QHostAddress target;
};

#endif // UDPOUTPUT_H
