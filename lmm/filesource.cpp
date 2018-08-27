#include "filesource.h"

#include <lmm/debug.h>
#include <lmm/circularbuffer.h>

#include <QDir>
#include <QHash>
#include <QFile>
#include <QFileInfo>

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
	file = NULL;
	QFileInfo finfo(filename);
	if (finfo.isFile()) {
		file = new QFile(filename);
		if (!file->open(QIODevice::ReadOnly))
			return -ENOENT;
		return BaseLmmElement::start();
	} else if (finfo.isDir()) {
		QDir d(filename);
		QStringList list = d.entryList(QDir::Files);
		foreach (QString str, list)
			dirEntries << d.absoluteFilePath(str);
		entryIndex = 0;
		return BaseLmmElement::start();
	}

	return -EINVAL;
}

int FileSource::stop()
{
	if (file) {
		fileLock.lock();
		if (file->isOpen())
			file->close();
		fileLock.unlock();
	}
	return BaseLmmElement::stop();
}

int FileSource::processBlocking(int ch)
{
	Q_UNUSED(ch);
	if (file)
		read(4096);
	else
		return readDir();
	return 0;
}

int FileSource::processBuffer(const RawBuffer &)
{
	return -EINVAL;
}

int FileSource::readDir()
{
	if (entryIndex >= dirEntries.size())
		return -ENOENT;

	QString fname = dirEntries[entryIndex];
	QFile f(fname);
	if (!f.open(QIODevice::ReadOnly))
		return -EPERM;
	QByteArray ba = f.readAll();
	f.close();

	RawBuffer buf("application/generic", (const void *)ba.constData(), ba.size());
	QHash<QString, QVariant> hash;
	hash["sourceFileName"] = fname;
	buf.pars()->metaData = RawBuffer::serializeMetadata(hash);
	buf.pars()->streamBufferNo = entryIndex;

	entryIndex++;

	return newOutputBuffer(0, buf);
}
