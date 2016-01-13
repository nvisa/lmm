#include "seiinserter.h"
#include "debug.h"

#include <lmm/dmai/h264encoder.h>
#include <lmm/tools/basesettinghandler.h>

#include <QFile>
#include <QDateTime>

#include <errno.h>

SeiInserter::SeiInserter(QObject *parent) :
	BaseLmmElement(parent),
	alarmID(1),
	insertSeiAlarm(false)
{
	seiAlarm = (AlarmProps){ALARM_IDLE, 0 ,300, 3000, 5000};
	seiActDur = 200;
	seiTimerStarted = false;

	BaseSettingHandler::addHandler("video_metadata", this);
}

int SeiInserter::setSeiProps(SeiInserter::AlarmType type, const QByteArray &data)
{
	lock.lock();
	incomingAlarmType = type;
	incomingSeiData = data;
	isSeiNew = true;
	lock.unlock();
	return 0;
}

void SeiInserter::setSeiField(RawBuffer buf)
{
	QByteArray seiData = incomingSeiData;
	if (isSeiNew == true) {
		isSeiNew = false;
		seiAlarm.alarmType = incomingAlarmType;
		insertSeiAlarm = true;
		if (seiAlarm.alarmType == ALARM_POST)
			seiTimerStarted = false;
	}
	if (seiAlarm.alarmType == ALARM_ONLINE) {
		buf.pars()->metaData = seiData;
	} else if (seiAlarm.alarmType == ALARM_STOP) {
		insertSeiAlarm = false;
		buf.pars()->metaData = seiData;
		seiAlarm.alarmType = ALARM_IDLE;
	} else if (seiAlarm.alarmType == ALARM_POST) {
		if (insertSeiAlarm == true) {
			if (seiTimerStarted == false) {
				seiTimer.start();
				seiTimerStarted = true;
			}
			buf.pars()->metaData = seiData;
		}
	}
}

int SeiInserter::processBuffer(const RawBuffer &buf)
{
	lock.lock();

	/* first check alarm duration */
	if (seiAlarm.alarmType == ALARM_POST && seiTimer.elapsed() > seiActDur) {
		insertSeiAlarm = false;
		seiAlarm.alarmType = ALARM_STOP;
		incomingSeiData.clear();
	}

	/* now insert sei data */
	setSeiField(buf);

	/* we are done */
	lock.unlock();

	return newOutputBuffer(0, buf);
}

void SeiInserter::setAlarmID()
{
	seiAlarm.alarmNumber = alarmID;
	alarmID++;
}

int SeiInserter::setSetting(const QString &setting, const QVariant &value)
{
	if (equals("video_metadata.sei._current_configuration")) {
		int seiDuration, alarmDuration, seiOffTime, insMode, seiActDur;
		insMode = BaseSettingHandler::getCache("video_metadata.sei.insertmode").toInt();
		seiDuration = BaseSettingHandler::getCache("video_metadata.sei.activeduration").toInt();
		alarmDuration = BaseSettingHandler::getCache("video_metadata.sei.alarmduration").toInt();
		seiOffTime = BaseSettingHandler::getCache("video_metadata.sei.offtime").toInt();
		seiActDur = BaseSettingHandler::getCache("video_metadata.sei.packetactivetime").toInt();
		mDebug("seiMode = %d, seiDuration = %d, alarmDuration = %d, seiOffTime = %d", insMode, seiDuration, alarmDuration, seiOffTime);
		this->seiActDur = seiActDur;
		return 0;
	} else if (starts("video_metadata.sei.")) {
		return 0;
	}

	return -ENOENT;
}

QVariant SeiInserter::getSetting(const QString &setting)
{
	if (equals("video_metadata.sei.insertmode")) {
		return 0;
	}
	if (equals("video_metadata.sei.insertmode")) {
		return seiAlarm.alarmType;
	}
	if (equals("video_metadata.sei.activeduration")) {
		return seiAlarm.seiDuration;
	}
	if (equals("video_metadata.sei.alarmduration")) {
		return seiAlarm.alarmDuration;
	}
	if (equals("video_metadata.sei.offtime")) {
		return seiAlarm.seiOfftime;
	}
	if (equals("video_metadata.sei.packetactivetime")) {
		return seiActDur;
	}

	return QVariant();
}
