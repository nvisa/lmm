#ifndef DEBUGSERVER_H
#define DEBUGSERVER_H

#include <QObject>
#include <QHash>

class QTcpServer;
class QTcpSocket;
class BaseLmmElement;

class DebugServer : public QObject
{
	Q_OBJECT
public:
	enum CustomStat {
		STAT_LOOP_TIME,
		STAT_OVERLAY_TIME,
		STAT_CAPTURE_TIME,
		STAT_ENCODE_TIME,
		STAT_RTSP_OUT_TIME,
		STAT_DISP_OUT_TIME,
	};
	explicit DebugServer(QObject *parent = 0);
	static int serverPortNo() { return 15748; }
	static void sendMessage(QTcpSocket *client, const QString &cmd,
							const QByteArray &data = QByteArray());
	void sendMessage(const QString &cmd, const char *msg);
	void setElements(QList<BaseLmmElement *> *list) { elements = list; }
	void addCustomStat(CustomStat stat, int val);
signals:
	
private slots:
	void clientArrived();
	void clientDisconnected();
	void clientDataReady();
private:
	QTcpServer *server;
	QTcpSocket *client;
	int blocksize;
	QHash<CustomStat, int> custom;

	QList<BaseLmmElement *> *elements;

	void handleMessage(QTcpSocket *client, const QString &cmd,
					   const QByteArray &data);
};

#endif // DEBUGSERVER_H
