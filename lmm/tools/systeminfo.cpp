#include "systeminfo.h"

#include "debug.h"

#include <QFile>
#include <QStringList>

#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef TARGET_ARM
#include <linux/i2c-dev.h>
#endif
#include <linux/i2c.h>

SystemInfo SystemInfo::inst;

#ifdef TARGET_ARM

static inline __s32 i2c_smbus_access(int file, char read_write, __u8 command,
									 int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;
	return ioctl(file,I2C_SMBUS,&args);
}

static inline __s32 i2c_smbus_write_byte_data(int file, __u8 command,
											  __u8 value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
							I2C_SMBUS_BYTE_DATA, &data);
}

static inline __u8 i2c_smbus_read_byte_data(int file, __u8 command)
{
	union i2c_smbus_data data;
	int status;
	status = i2c_smbus_access(file, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA,&data);
	return (status < 0) ? status : data.byte;
}

int SystemInfo::getTVPVersion()
{
	int fd = open("/dev/i2c-1", O_RDWR);

	/* With force, let the user read from/write to the registers
		 even when a driver is also running */
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x5d) < 0) {
		fprintf(stderr,
				"Error: Could not set address to 0x%02x: %s\n",
				0x5f, strerror(errno));
		return -errno;
	}

	short ver = i2c_smbus_read_byte_data(fd, 0x80) << 8;
	ver |= i2c_smbus_read_byte_data(fd, 0x81);
	::close(fd);
	return ver;
}

int SystemInfo::getADV7842Version()
{
	int fd = open("/dev/i2c-1", O_RDWR);

	/* With force, let the user read from/write to the registers
		 even when a driver is also running */
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x20) < 0) {
		fprintf(stderr,
				"Error: Could not set address to 0x%02x: %s\n",
				0x20, strerror(errno));
		return -errno;
	}

	short ver = i2c_smbus_read_byte_data(fd, 0xea) << 8;
	ver |= i2c_smbus_read_byte_data(fd, 0xeb);
	::close(fd);
	return ver;
}

int SystemInfo::getTFP410DevId()
{
	int fd = open("/dev/i2c-1", O_RDWR);

	/* With force, let the user read from/write to the registers
		 even when a driver is also running */
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x3f) < 0) {
		fprintf(stderr,
				"Error: Could not set address to 0x%02x: %s\n",
				0x3f, strerror(errno));
		return -errno;
	}

	short ver = i2c_smbus_read_byte_data(fd, 0x03) << 8;
	ver |= i2c_smbus_read_byte_data(fd, 0x02);
	::close(fd);
	return ver;
}

#else

int SystemInfo::getTVPVersion()
{
	return -ENOENT;
}

int SystemInfo::getADV7842Version()
{
	return -ENOENT;
}

int SystemInfo::getTVP5158Version(int addr)
{
	return -ENOENT;
}

#endif

SystemInfo::SystemInfo(QObject *parent) :
	QObject(parent)
{
	meminfo = NULL;
	memtime.start();
}

int SystemInfo::getUptime()
{
	QFile f("/proc/uptime");
	if (!f.open(QIODevice::ReadOnly | QIODevice::Unbuffered))
		return -EPERM;
	QList<QByteArray> list = f.readLine().split(' ');
	return list[0].toDouble();
}

int SystemInfo::getProcFreeMemInfo()
{
	if (memtime.elapsed() < 1000)
		return lastMemFree;
	if (!meminfo) {
		meminfo = new QFile("/proc/meminfo");
		meminfo->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
	} else
		meminfo->seek(0);
	QStringList lines = QString::fromUtf8(meminfo->readAll()).split("\n");
	int free = 0;
	foreach (const QString line, lines) {
		/*
		 * 2 fields are important, MemFree and Cached
		 */
		if (line.startsWith("MemFree:"))
			free += line.split(" ", QString::SkipEmptyParts)[1].toInt();
		else if (line.startsWith("Cached:")) {
			free += line.split(" ", QString::SkipEmptyParts)[1].toInt();
			break;
		}
	}
	memtime.restart();
	/* prevent false measurements */
	if (free)
		lastMemFree = free;
	return lastMemFree;
}

int SystemInfo::getFreeMemory()
{
	return inst.getProcFreeMemInfo();
}

int SystemInfo::getTVP5158Version(int addr)
{
	int fd = open("/dev/i2c-1", O_RDWR);

	/* With force, let the user read from/write to the registers
		 even when a driver is also running */
	if (ioctl(fd, I2C_SLAVE_FORCE, addr) < 0) {
		fprintf(stderr,
				"Error: Could not set address to 0x%02x: %s\n",
				addr, strerror(errno));
		return -errno;
	}

	short ver = i2c_smbus_read_byte_data(fd, 0x8) << 8;
	ver |= i2c_smbus_read_byte_data(fd, 0x9);
	::close(fd);
	return ver;
}
