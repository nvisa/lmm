#include "seiinserter.h"
#include "debug.h"

#include <lmm/dmai/h264encoder.h>

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
