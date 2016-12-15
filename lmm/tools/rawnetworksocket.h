#ifndef RAWNETWORKSOCKET_H
#define RAWNETWORKSOCKET_H

#include <QList>
#include <QString>

struct sockaddr_in;

class RawNetworkSocket
{
public:
	struct SockBuffer {
		char *data;
		char *payload;
		int size;
		int index;
	};

	RawNetworkSocket(QString destinationIp, quint16 destinationPort, QString sourceIp, quint16 sourcePort, int ttl);
	~RawNetworkSocket();
	bool isActive();
	int send(char *buf, int size);
	int send(SockBuffer *sbuf, int size);
	SockBuffer * getNextFreeBuffer();
	int socketDescriptor();
protected:
	int fd;
	QString dstIp;
	QString myIp;
	quint16 dstPort;
	quint16 srcPort;
	struct sockaddr_in *sin;
	QList<SockBuffer *> buffers;
	int nextIndex;

	int init();
	int sendData(char *datagram, int size);
	SockBuffer *createBuffer(int ttl);
};

#endif // RAWNETWORKSOCKET_H
