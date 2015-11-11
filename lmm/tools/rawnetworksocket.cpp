#include "rawnetworksocket.h"

#include <lmmcommon.h>

#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

/*
	96 bit (12 bytes) pseudo header needed for udp header checksum calculation
*/
struct pseudo_header
{
	u_int32_t source_address;
	u_int32_t dest_address;
	u_int8_t placeholder;
	u_int8_t protocol;
	u_int16_t udp_length;
};

static unsigned short csum(unsigned short *ptr,int nbytes)
{
	register long sum;
	unsigned short oddbyte;
	register short answer;

	sum=0;
	while(nbytes>1) {
		sum+=*ptr++;
		nbytes-=2;
	}
	if(nbytes==1) {
		oddbyte=0;
		*((u_char*)&oddbyte)=*(u_char*)ptr;
		sum+=oddbyte;
	}

	sum = (sum>>16)+(sum & 0xffff);
	sum = sum + (sum>>16);
	answer=(short)~sum;

	return(answer);
}

RawNetworkSocket::RawNetworkSocket(QString destinationIp, quint16 destinationPort, QString sourceIp, quint16 sourcePort)
{
	dstIp = destinationIp;
	dstPort = destinationPort;
	srcPort = sourcePort;
	myIp = sourceIp;
	fd = init();
	for (int i = 0; i < 128; i++)
		buffers << createBuffer();
	nextIndex = -1;
}

RawNetworkSocket::~RawNetworkSocket()
{
	for (int i = 0; i < buffers.size(); i++) {
		delete [] buffers[i]->data;
		delete buffers[i];
	}
	buffers.clear();
	close(fd);
	delete sin;
}

bool RawNetworkSocket::isActive()
{
	if (fd < 0)
		return false;
	return true;
}

int RawNetworkSocket::send(char *buf, int size)
{
	SockBuffer *sbuf = getNextFreeBuffer();
	char *datagram = sbuf->data;
	char *data = datagram + sizeof(struct iphdr) + sizeof(struct udphdr);
	memcpy(data , buf, size);
	return sendData(sbuf->data, size);
}

int RawNetworkSocket::send(RawNetworkSocket::SockBuffer *sbuf, int size)
{
	if (LmmCommon::isFaultInjected(Lmm::FI_RANDOM_RTP_PACKET_DROP)) {
		int r = rand() % 10;
		if (r == 2)
			return 0;
	}
	return sendData(sbuf->data, size);
}

RawNetworkSocket::SockBuffer * RawNetworkSocket::getNextFreeBuffer()
{
	nextIndex++;
	if (nextIndex >= buffers.size())
		nextIndex = 0;
	return buffers[nextIndex];
}

int RawNetworkSocket::socketDescriptor()
{
	return fd;
}

int RawNetworkSocket::init()
{
	int fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if(fd == -1)
		return -errno;

	sin = new sockaddr_in;
	sin->sin_family = AF_INET;
	sin->sin_port = htons(srcPort);
	sin->sin_addr.s_addr = inet_addr(qPrintable(dstIp));

	return fd;
}

int RawNetworkSocket::sendData(char *datagram, int size)
{
	struct iphdr *iph = (struct iphdr *) datagram;
	struct udphdr *udph = (struct udphdr *) (datagram + sizeof (struct ip));

	iph->tot_len = sizeof (struct iphdr) + sizeof (struct udphdr) + size;
	udph->len = htons(8 + size); //udp header size

	//Ip checksum
	iph->check = csum ((unsigned short *) datagram, iph->tot_len);
#if 0
	char *pseudogram;
	struct pseudo_header psh;
	//Now the UDP checksum using the pseudo header
	psh.source_address = inet_addr("192.168.1.130");
	psh.dest_address = sin.sin_addr.s_addr;
	psh.placeholder = 0;
	psh.protocol = IPPROTO_UDP;
	psh.udp_length = htons(sizeof(struct udphdr) + size);

	int psize = sizeof(struct pseudo_header) + sizeof(struct udphdr) + size;
	pseudogram = (char *)malloc(psize);

	memcpy(pseudogram , (char*) &psh , sizeof (struct pseudo_header));
	memcpy(pseudogram + sizeof(struct pseudo_header) , udph , sizeof(struct udphdr) + size);

	udph->check = csum( (unsigned short*) pseudogram , psize);
#endif
	return sendto (fd, datagram, iph->tot_len ,  0, (struct sockaddr *)sin, sizeof (struct sockaddr_in));
}

RawNetworkSocket::SockBuffer * RawNetworkSocket::createBuffer()
{
	//Datagram to represent the packet
	char *datagram = new char[4096];
	//zero out the packet buffer
	memset (datagram, 0, 4096);

	//IP header
	struct iphdr *iph = (struct iphdr *) datagram;

	//UDP header
	struct udphdr *udph = (struct udphdr *) (datagram + sizeof (struct ip));

	//Fill in the IP Header
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;
	iph->id = htonl(0); //Id of this packet
	iph->frag_off = 0;
	iph->ttl = 255;
	iph->protocol = IPPROTO_UDP;
	iph->check = 0;
	iph->saddr = inet_addr(qPrintable(myIp));
	iph->daddr = inet_addr(qPrintable(dstIp));

	//UDP header
	udph->source = htons(srcPort);
	udph->dest = htons(dstPort);
	udph->check = 0;

	SockBuffer *sbuf = new SockBuffer;
	sbuf->data = datagram;
	sbuf->payload = sbuf->data + sizeof(struct iphdr) + sizeof(struct udphdr);
	sbuf->index = 0;
	sbuf->size = 4096;
	return sbuf;
}
