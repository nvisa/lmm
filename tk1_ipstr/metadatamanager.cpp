#include "metadatamanager.h"

#include "lmm/debug.h"
#include "net/tcpserver.h"
#include "net/udpserver.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QXmlStreamWriter>
#include <QDateTime>
#include <QDir>

MetaDataManager::MetaDataManager(quint16 tcpPort, quint16 udpPort, quint16 httpPort, QObject *parent)
	: QObject(parent)
{
	// initializing
	uMeta = NULL;
	state = false;
	ucData = false;
	stData = false;

	metaDataKeys = initMetaStruct();

	quint16 tPort = tcpPort;
	quint16 uPort = udpPort;
	quint16 hPort = httpPort;

	udpServer = new UdpServer(uPort);
	tcpServer = new TcpServer(tPort);
	httpServer = new SimpleHttpServer(hPort);

	whichServer = 2; // 0 = tcp, 1 = http , 2 = udp

	timer = new QTimer();
	timer->start(1000);
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
}

QStringList MetaDataManager::initMetaStruct()
{
	return QStringList() << "ColorSpace" << "Shadow" << "Ill" << "Debug" << "FPS";
}

void MetaDataManager::timeout()
{
	timer->setInterval(1000);
	QByteArray ba;
	if(ucData) {
		ba = QByteArray((char*)uMeta);
	} else if(stData) {
		ba = convertStructToJson(sMeta);
		ba = convertStructToByteArray(sMeta);
		ba = convertStructToXml(sMeta);
	}
	if(!ba.isEmpty()) {
		state = state | tcpServer->sendMetaData(ba);
		state = state | udpServer->sendMetaData(ba);
		state = state | httpServer->sendMetaData(ba);
	} else {
		mDebug("alarm file is empty");
	}
	if(state) {
		ucData = false;
		stData = false;
	}
}

void MetaDataManager::sendMetaData(uchar *meta)
{
	uMeta = meta;
	// writeDataToFile();
	ucData = true;
}

void MetaDataManager::sendMetaData(MetaData meta_data)
{
	sMeta = meta_data;
	stData = true;
}

QByteArray MetaDataManager::convertStructToByteArray(MetaData meta_data)
{
	QByteArray ba;
	foreach (QString stKey, metaDataKeys) {
		if(stKey.contains("ColorSpace")) {
			ba += "ColorSpace = " + meta_data.colorspace + "\n";
			continue;
		}
		if(stKey.contains("Shadow")) {
			ba += "Shadow = " + meta_data.shadow + "\n";
			continue;
		}
		if(stKey.contains("Ill")) {
			ba += "Ill = " + meta_data.ill + "\n";
			continue;
		}
		if(stKey.contains("Debug")) {
			ba += "Debug = " + meta_data.debug + "\n";
			continue;
		}
		if(stKey.contains("Fps")) {
			ba += "FPS = " + QString::number(meta_data.fps).toLocal8Bit() + "\n";
			continue;
		}
	}
	return ba;
}

QByteArray MetaDataManager::convertStructToXml(MetaData meta_data)
{
	QByteArray ba;
	QXmlStreamWriter xml(&ba);

	xml.setAutoFormatting(true);

	xml.setCodec("UTF-8");
	xml.writeStartDocument();

	xml.writeStartElement("s:Envelope");
	xml.writeAttribute("xmlns:s", "http://schemas.xmlsoap.org/soap/envelope/");

	xml.writeStartElement("s:Body");

	xml.writeStartElement("Login");
	xml.writeAttribute("xmlns", "http://tempuri.org/");

	foreach (QString stKey, metaDataKeys) {
		if(stKey.contains("ColorSpace")) {
			xml.writeTextElement("ColorSpace", meta_data.colorspace);
			continue;
		}
		if(stKey.contains("Shadow")) {
			xml.writeTextElement("Shadow", meta_data.shadow);
			continue;
		}
		if(stKey.contains("Ill")) {
			xml.writeTextElement("Ill", meta_data.ill);
			continue;
		}
		if(stKey.contains("Debug")) {
			xml.writeTextElement("Debug", meta_data.debug);
			continue;
		}
		if(stKey.contains("Fps")) {
			xml.writeTextElement("Fps", QString::number(meta_data.fps));
			continue;
		}
	}

	xml.writeEndElement(); // login
	xml.writeEndElement(); // Body
	xml.writeEndElement(); // Envelope

	xml.writeEndDocument();
	return ba;
}

QByteArray MetaDataManager::convertStructToJson(MetaData meta_data)
{
	QJsonObject obj;
	foreach (QString stKey, metaDataKeys) {
		if(stKey.contains("ColorSpace")) {
			obj.insert(stKey, QJsonValue(meta_data.colorspace));
			continue;
		}
		if(stKey.contains("Shadow")) {
			obj.insert(stKey, QJsonValue(meta_data.shadow));
			continue;
		}
		if(stKey.contains("Ill")) {
			obj.insert(stKey, QJsonValue(meta_data.ill));
			continue;
		}
		if(stKey.contains("Debug")) {
			obj.insert(stKey, QJsonValue(meta_data.debug));
			continue;
		}
		if(stKey.contains("Fps")) {
			obj.insert(stKey, QJsonValue(meta_data.fps));
			continue;
		}
	}
	QJsonDocument json(obj);
	return json.toJson();
}

MetaData MetaDataManager::newMetaData()
{
	MetaData m;
	return m;
}

void MetaDataManager::writeDataToFile(QByteArray data)
{
	QDir d; d.mkdir("alarms");
	QFile f(QString("alarms/alarm_info_%1.txt").arg(QDateTime::currentDateTime().toString()));
	if(!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
		mDebug("Error saving alarm file");
		return;
	}

	QTextStream in(&f);
	in << data << "\n";
	f.close();
}
