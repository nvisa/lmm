#include "baseplayer.h"
#include "tools/remoteconsole.h"
#include "tools/basesettinghandler.h"

#include <lmm/debug.h>
#include <lmm/streamtime.h>

#include <errno.h>

BasePlayer::BasePlayer(QObject *parent) :
	BaseLmmElement(parent)
{
	streamTime = new StreamTime;
	connect(&timer, SIGNAL(timeout()), SLOT(timeout()));
	BaseSettingHandler::addTarget(this);
}

int BasePlayer::startElement(BaseLmmElement *el)
{
	mInfo("starting element %s", el->metaObject()->className());
	el->setTotalInputBufferSize(0, 0);
	el->flush();
	el->setStreamTime(streamTime);
	el->setStreamDuration(-1);
	/* start element */
	int err = el->start();
	if (err) {
		mDebug("error %d starting element %s", err, el->metaObject()->className());
		return err;
	}
	return elementStarted(el);
}

int BasePlayer::start()
{
	if (getState() != INIT && getState() != STOPPED)
		return -EINVAL;
	int err;
	foreach (BaseLmmElement *el, elements) {
		err = startElement(el);
		if (err) {
			mDebug("error starting element %s", el->metaObject()->className());
			return err;
		}
	}
	streamTime->start();

	err = startPlayer();
	if (err) {
		mDebug("start error %d", err);
		return err;
	}

	manCons = createManagementConsole();
	return BaseLmmElement::start();
}

int BasePlayer::stop()
{
	int err;
	err = stopPlayer();
	if (err) {
		mDebug("stop error %d", err);
		return err;
	}

	/*foreach (BaseLmmElement *el, elements) {
		mDebug("stopping element %s", el->metaObject()->className());
		el->stop();
	}*/

	mDebug("playback stopped");
	return BaseLmmElement::stop();
}

int BasePlayer::pause()
{
	return -EPERM;
}

int BasePlayer::resume()
{
	return -EPERM;
}

qint64 BasePlayer::getDuration()
{
	return -1;
}

qint64 BasePlayer::getPosition()
{
	return -1;
}

int BasePlayer::seekTo(qint64)
{
	return -EPERM;
}

int BasePlayer::seek(qint64)
{
	return -EPERM;
}

int BasePlayer::setMute(bool)
{
	return -EPERM;
}

int BasePlayer::setVolumeLevel(int per)
{
	return -EPERM;
}

int BasePlayer::getVolumeLevel()
{
	return -EPERM;
}

bool BasePlayer::isEOF()
{
	return false;
}

int BasePlayer::setSourceUrl(QUrl url)
{
	sourceUrl = url;
	return 0;
}

int BasePlayer::setFilename(QString filename)
{
	return setSourceUrl(QUrl(filename));
}

void BasePlayer::timeout()
{
}

int BasePlayer::elementStarted(BaseLmmElement *el)
{
	return 0;
}

RemoteConsole *BasePlayer::createManagementConsole()
{
	return new RemoteConsole(this);
}
