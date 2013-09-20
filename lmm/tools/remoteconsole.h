#ifndef REMOTECONSOLE_H
#define REMOTECONSOLE_H

#include <QObject>

class QUdpSocket;

class RemoteConsole : public QObject
{
	Q_OBJECT
public:
	explicit RemoteConsole(int port = 8945, QObject *parent = 0);
signals:
	
private slots:
	void readPendingDatagrams();
protected:
	QUdpSocket *sock;

	QByteArray processMessage(QByteArray mes);
	QString processMessage(QString mes);
};

#endif // REMOTECONSOLE_H
