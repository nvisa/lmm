#include "metadatagenerator.h"

#include <lmm/debug.h>
#include <lmm/tools/cpuload.h>

#include <ecl/drivers/systeminfo.h>
#include <ecl/settings/applicationsettings.h>

#include <QFile>
#include <QDataStream>

static const QString getMacAddress(const QString &iface)
{
	QFile f(QString("/sys/class/net/%1/address").arg(iface));
	if (!f.open(QIODevice::ReadOnly))
		return "ff:ff:ff:ff:ff:ff";
	return QString::fromUtf8(f.readAll()).trimmed();
}

MetadataGenerator::MetadataGenerator(QObject *parent) :
	BaseLmmElement(parent)
{
	macAddr = getMacAddress("eth0");

	ApplicationSettings *s = ApplicationSettings::create("/etc/encsoft/device_info.json", QIODevice::ReadOnly);
	appVersion = s->get("version_information.application_version").toString();
	superVersion = s->get("version_information.encodertools_version").toString();
	fwVersion = s->get("version_information.firmware_version").toString();
	hwVersion = s->get("version_information.hardware_id").toString();
	serialNo = s->get("version_information.serial_no").toString();
	streamerVersion = s->get("version_information.lmm_library_version").toString();
}

int MetadataGenerator::processBlocking(int ch)
{
	Q_UNUSED(ch);

	sleep(1);

	QByteArray ba;
	QDataStream out(&ba, QIODevice::WriteOnly);
	out.setByteOrder(QDataStream::LittleEndian);
	out.setFloatingPointPrecision(QDataStream::SinglePrecision);

	out << (qint32)0x78984578;
	out << (qint32)0x101; //version

	out << (qint32)CpuLoad::getAverageCpuLoad();
	out << (qint32)SystemInfo::getFreeMemory();
	out << (qint32)SystemInfo::getUptime();
	out << macAddr;
	out << appVersion;
	out << superVersion;
	out << fwVersion;
	out << hwVersion;
	out << serialNo;
	out << streamerVersion;

	return newOutputBuffer(RawBuffer("application/x-metadata", ba.constData(), ba.size()));
}

int MetadataGenerator::processBuffer(const RawBuffer &buf)
{
	Q_UNUSED(buf);
	return 0;
}
