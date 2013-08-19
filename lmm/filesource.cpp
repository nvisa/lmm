#include "filesource.h"

#include <lmm/debug.h>
#include <lmm/circularbuffer.h>

#include <QFile>

#include <errno.h>

FileSource::FileSource(QObject *parent) :
	BaseLmmElement(parent)
{
}

int FileSource::read(int size)
{
	RawBuffer buf("unknown", size);
	fileLock.lock();
	int len = -1;
	if (file->isOpen())
		len = file->read((char *)buf.data(), size);
	fileLock.unlock();
	if (len <= 0)
		return len;
	else if (len < size) {
		RawBuffer buf2("unknown", len);
		memcpy(buf2.data(), buf.data(), len);
		buf = buf2;
	}
	newOutputBuffer(0, buf);
	return len;
}

int FileSource::start()
{
	file = new QFile(filename);
	if (!file->open(QIODevice::ReadOnly))
		return -ENOENT;
	return BaseLmmElement::start();
}

int FileSource::stop()
{
	fileLock.lock();
	if (file->isOpen())
		file->close();
	fileLock.unlock();
	return BaseLmmElement::stop();
}

int FileSource::processBuffer(RawBuffer)
{
	return -EINVAL;
}
