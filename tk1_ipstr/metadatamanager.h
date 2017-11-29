#ifndef METADATAMANAGER_H
#define METADATAMANAGER_H

#include <QTimer>
#include <QObject>
#include <QStringList>
class TcpServer;
class UdpServer;
class SimpleHttpServer;

struct MetaData {
	QString colorspace;
	QString shadow;
	QString ill;
	QString debug;
	double fps;
};

class MetaDataManager : public QObject
{
	Q_OBJECT
public:
	explicit MetaDataManager(quint16 tcpPort = 5778, quint16 udpPort = 13456, quint16 httpPort = 9999, QObject *parent = 0);
	void sendMetaData(uchar *meta);
	void sendMetaData(MetaData meta);
	MetaData newMetaData();
signals:

public slots:
protected:
	QByteArray convertStructToByteArray(MetaData meta_data);
	QByteArray convertStructToXml(MetaData meta_data);
	QByteArray convertStructToJson(MetaData meta_data);
	QStringList initMetaStruct();
	void writeDataToFile(QByteArray data);
protected slots:
	void timeout();
private:
	QTimer *timer;
	UdpServer *udpServer;
	TcpServer *tcpServer;
	SimpleHttpServer *httpServer;
	MetaData sMeta;
	uchar *uMeta;
	QStringList metaDataKeys;
	bool ucData;
	bool stData;
	int whichServer;
	bool state;
};

#endif // METADATAMANAGER_H
