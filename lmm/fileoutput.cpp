#include "fileoutput.h"
#include "rawbuffer.h"
#include "lmmcommon.h"

#include "emdesk/debug.h"

#include <QFile>
#include <QtConcurrentRun>

#include <errno.h>

FileOutput::FileOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	watcher = new QFutureWatcher<int>;
	file = NULL;
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
	if (file) {
		file->close();
		delete file;
	}

	return BaseLmmElement::stop();
}

int FileOutput::outputFunc()
{
	if (isPipe)
		return fifoOutput();
	mInfo("buffer count: %d", inputBuffers.count());
	if (!inputBuffers.size())
		return -ENOENT;
	RawBuffer buf = inputBuffers.takeFirst();
	/* We don't do any syncing */
	return writeBuffer(buf);
}

void FileOutput::setFileName(QString name, bool pipe)
{
	fileName = name;
	isPipe = pipe;
	pipeClosed = false;
}

void FileOutput::signalReceived(int)
{
	pipeClosed = true;
}

int FileOutput::writeBuffer(RawBuffer buf)
{
	if (pipeClosed) {
		file->close();
		pipeClosed = false;
	}
	if (!file->isOpen()) {
		if (!file->open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
			mDebug("error opening output file %s", qPrintable(file->fileName()));
			return -EINVAL;
		}
	}
	const char *data = (const char *)buf.constData();
	int written = file->write(data, buf.size());
	if (written < 0) {
		mDebug("error writing to output file %s", qPrintable(file->fileName()));
		return -EIO;
	}
	while (written < buf.size()) {
		int err = file->write(data + written, buf.size() - written);
		if (err < 0) {
			mDebug("error writing to output file %s, %d bytes written",
				   qPrintable(file->fileName()), written);
			return -EIO;
		}
		written += err;
	}
	mInfo("%d bytes written", buf.size());
	return 0;
}

int FileOutput::fifoOutput()
{
	if (inputBuffers.size() > 1)
		mDebug("%d buffers", inputBuffers.size());
	while (inputBuffers.size()) {
		if (!watcher->future().isFinished())
			break;
		RawBuffer buf = inputBuffers.takeFirst();
		watcher->setFuture(QtConcurrent::run(this, &FileOutput::writeBuffer, buf));
	}
	return 0;
}
