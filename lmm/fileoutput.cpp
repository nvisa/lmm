#include "fileoutput.h"
#include "rawbuffer.h"
#include "lmmcommon.h"

#include "debug.h"

#include <QFile>
#include <QDateTime>
#include <QtConcurrentRun>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

FileOutput::FileOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	file = NULL;
	incremental = false;
	setFileName("fileout");
	LmmCommon::registerForPipeSignal(this);
}

int FileOutput::start()
{
	file = new QFile(fileName);
	if (!isPipe) {
		if (!file->open(QIODevice::ReadWrite | QIODevice::Truncate)) {
			mDebug("error opening output file %s", qPrintable(file->fileName()));
			return -EINVAL;
		}
	}

	return BaseLmmOutput::start();
}

int FileOutput::stop()
{
	mutex.lock();
	if (file) {
		file->close();
		delete file;
	}
	mutex.unlock();

	return BaseLmmElement::stop();
}

int FileOutput::outputBuffer(RawBuffer buf)
{
	int err = writeBuffer(buf);
	if (incremental) {
		mutex.lock();
		file->close();
		file->setFileName(QDateTime::currentDateTime().toString("ddMMyyyy_hhmmss_").append(fileName));
		mutex.unlock();
	}
	return err;
}

void FileOutput::setFileName(QString name)
{
	struct stat stats;
	stat(qPrintable(name), &stats);
	fileName = name;
	isPipe = S_ISFIFO(stats.st_mode);
	pipeClosed = false;
	if (getState() == STARTED) {
		mutex.lock();
		file->close();
		mutex.unlock();
	}
}

void FileOutput::signalReceived(int)
{
	pipeClosed = true;
}

int FileOutput::writeBuffer(RawBuffer buf)
{
	mutex.lock();
	if (pipeClosed) {
		file->close();
		pipeClosed = false;
	}
	if (!file->isOpen()) {
		if (!file->open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
			mDebug("error opening output file %s", qPrintable(file->fileName()));
			mutex.unlock();
			return -EINVAL;
		}
	}
	const char *data = (const char *)buf.constData();
	int written = file->write(data, buf.size());
	if (written < 0) {
		mDebug("error writing to output file %s", qPrintable(file->fileName()));
		mutex.unlock();
		return -EIO;
	}
	while (written < buf.size()) {
		int err = file->write(data + written, buf.size() - written);
		if (err < 0) {
			mDebug("error writing to output file %s, %d bytes written",
				   qPrintable(file->fileName()), written);
			mutex.unlock();
			return -EIO;
		}
		written += err;
	}
	mutex.unlock();
	mInfo("%d bytes written", buf.size());
	return 0;
}
