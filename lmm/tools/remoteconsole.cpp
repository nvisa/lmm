#include "remoteconsole.h"
#include "basesettinghandler.h"
#include "debug.h"

#include <QUdpSocket>
#include <QStringList>

RemoteConsole::RemoteConsole(int port, QObject *parent) :
	QObject(parent)
{
	sock = new QUdpSocket(this);
	sock->bind(QHostAddress::Any, port);

	connect(sock, SIGNAL(readyRead()), SLOT(readPendingDatagrams()));
}

void RemoteConsole::readPendingDatagrams()
{
	while (sock->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);
		QByteArray ba = processMessage(datagram);
		if (ba.size())
			sock->writeDatagram(ba, sender, senderPort);
			//sock->writeDatagram(ba.append("\n>"), sender, senderPort);
	}
}

QByteArray RemoteConsole::processMessage(QByteArray mes)
{
	return processMessage(QString::fromUtf8(mes)).toUtf8();
}

static QString cerror(int code = 0)
{
	return "";
}

QString RemoteConsole::processMessage(QString mes)
{
	if (mes.trimmed().isEmpty())
		return cerror();
	QString resp;
	QStringList fields = mes.trimmed().split(" ", QString::SkipEmptyParts);
	if (fields.size() > 1 && fields[0] == "get") {
		QString set = fields[1].trimmed();
		resp = QString("get:%1:%2").arg(set).arg(BaseSettingHandler::getSettingString(set));
	} else if (fields.size() > 2 && fields[0] == "set") {
		QString set = fields[1].trimmed();
		int err = BaseSettingHandler::setSetting(set, fields[2].trimmed());
		resp = QString("set:%1:%2").arg(set).arg(err);
	}
	return resp;
}
