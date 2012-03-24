#include "fileoutput.h"
#include "rawbuffer.h"
#include "emdesk/debug.h"

#include <QFile>

#include <errno.h>
FileOutput::FileOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	file = NULL;
}

int FileOutput::start()
{
	file = new QFile("fileout");
	if (!file->open(QIODevice::ReadWrite | QIODevice::Truncate)) {
		mDebug("error opening output file %s", qPrintable(file->fileName()));
		return -EINVAL;
	}

	return BaseLmmElement::start();
}

int FileOutput::stop()
{
	if (file) {
		file->close();
		delete file;
	}

	return BaseLmmElement::stop();
}

int FileOutput::output()
{
	mInfo("buffer count: %d", inputBuffers.count());
	if (!inputBuffers.size())
		return -ENOENT;
	RawBuffer buf = inputBuffers.takeFirst();
	/* We don't do any syncing */
	const char *data = (const char *)buf.constData();
	int written = file->write(data, buf.size());
	if (written < 0) {
		mDebug("error writing to output file %s", qPrintable(file->fileName()));
		return -EIO;
	}
	while (written < buf.size()) {
		int err = file->write(data + written, buf.size() - written);
		if (err < 0) {
			mDebug("error writing to output file %s, %d bytes written", qPrintable(file->fileName()), written);
			return -EIO;
		}
		written += err;
	}
	mInfo("%d bytes written", buf.size());
	return 0;
}
