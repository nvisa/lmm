#include "seiinserter.h"

#include <ecl/drivers/gpiocontroller.h>

#include <lmm/debug.h>
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
	motprov = NULL;
	gpio = NULL;
	qsrand(uint(QDateTime::currentDateTime().toTime_t() & 0xFFFF));
}

void SeiInserter::setMotionDetectionProvider(MotionDetectionInterface *iface)
{
	motprov = iface;
}

int SeiInserter::setAlarmTemplate(const QString &filename)
{
	QFile f(filename);
	if (!f.open(QIODevice::ReadOnly))
		return -ENOENT;
	info.alarmTemplate = QString::fromUtf8(f.readAll());
	f.close();
	return 0;
}

int SeiInserter::setSeiProps(SeiInserter::AlarmType type, const QByteArray &data)
{
	lock.lock();
	info.algorithm = 0;
	incomingAlarmType = type;
	incomingSeiData = data;
	isSeiNew = true;
	lock.unlock();
	return 0;
}

int SeiInserter::addGpioAlarm(int io)
{
	if (!gpio)
		gpio = new GpioController(this);
	int err = gpio->requestGpio(io, GpioController::INPUT);
	if (err)
		return err;
	if (!info.ios.contains(io))
		info.ios << io;
	return 0;
}

int SeiInserter::setAlarmInformation(const QString &templateFile, int alg, int gpioId, int ioLevel, int minDuration, int motThreshold)
{
	int err = setAlarmTemplate(templateFile);
	if (err)
		return err;
	info.alarmTemplateFilename = templateFile;
	info.algorithm = alg;
	info.enabled = true;
	info.ioAlarmLevel = ioLevel;
	info.minAlarmDuration = minDuration;
	info.motionAlarmThreshold = motThreshold * NORMALIZE_MOTION;
	info.motionAlarmThrPercent = motThreshold;

	if (gpioId >= 0)
		addGpioAlarm(gpioId);
	return 0;
}

const QByteArray SeiInserter::generateAlarm(const RawBuffer &buf)
{
#define LINK_ALARM_TIME 5000
	QByteArray ba;
	int activeio;
	/* we will generate our own alarms */
	if (info.algorithm == 1) {
		int alarmActive = 0;
		static int alarmSource = 0;
		int motv = 0;
		int motRegs = 0;
		if (motprov) {
			motv = motprov->getMotionValue();
			motRegs = motprov->getMotionRegions();
		}
		if (motv == 0)
			/* this one is I-frame */
			motRegs = info.lastMotionValue;
		info.lastMotionValue = motRegs;
		int ioState = 0;

		/* we shouldn't generate alarms more often than minAlarmDuration*/
		bool canTrigger = true;
		if (info.current.active && info.current.t.elapsed() < info.minAlarmDuration)
			canTrigger = false;

		if (canTrigger) {
			/* let's first check motion alarm */
			//if (motv > info.motionAlarmThreshold) {
			if (motRegs > 0) {
				alarmActive = 1;
				alarmSource = info.current.source | 0x1;

				if (info.current.active == false
						|| info.current.source != alarmSource) {
					/* we should generate a new alarm */
					info.current.triggerNew(alarmSource);
				}
				linkTimer.start();
#if 0
				else if (info.current.t.elapsed() > info.minAlarmDuration) {
					/* we should trigger a new alarm */
					info.current.triggerNew(alarmSource);
				}
#endif
			} else if (info.current.active && info.current.source != 2 && linkTimer.elapsed() > LINK_ALARM_TIME) {
				/* motion alarm should suppress */
				info.current.suppress();
				alarmSource = 0;
			}

			/* then we check IO alarms */
			foreach (int io, info.ios) {
				int val = gpio->getGpioValue(io);
				if (info.ioAlarmLevel == val) {
					activeio = io;
					alarmActive = 1;
					alarmSource = alarmSource | 0x2;
					if (info.current.active == false
							|| info.current.source != alarmSource)
						info.current.triggerNew(alarmSource);
#if 0
					else if (info.current.t.elapsed() > info.minAlarmDuration)
						info.current.triggerNew(alarmSource);
#endif
				} else {
					info.current.source = alarmSource & ~0x02;
					if (info.current.source == 0)
						info.current.suppress();
				}
				ioState = val;
			}
		}
		QByteArray sign = ((RawBuffer *)&buf)->pars()->metaData.toHex();

		if (!info.current.active) {
			QString templ = "<?xml version='1.0' encoding='UTF-8'?><SEI><SIGN>%1</SIGN></SEI>";
			return templ.arg(sign.constData()).toUtf8();
		}
#if 0
		motv /= NORMALIZE_MOTION;
		if (motv > 100)
			motv = 100;
#endif
		bool ok;
		ba = info.alarmTemplate
				/*
				* We have 14 fields in our template file:
				*	1st: current date and time
				*	2nd: alarm ID if alarm is present for text overlay
				*	3rd: current date and time for text overlay
				*	4th: alarm source if alarm is present - 0: no alarm, 1: motion alarm, 2: gpio alarm, 3: both
				*	5th: 16-bit number showing the regions that has motion for text overlay
				*	6th: motion detection threshold
				*	7th: IO-1 output state
				*	8th: alarm ID if alarm is present for NVR
				*	9th: alarm source if alarm is present - 0: no alarm, 1: motion alarm, 2: gpio alarm, 3: both
				*	10th: 16-bit number showing the regions that has motion
				*	11th: motion detection threshold
				*	12th: IO-1 output state
				*	13th: 16-bit number showing the regions that has motion
				*	14th: frame signature based on MD5 for hash control
				*/
				.arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"))
				.arg(info.current.id.toLongLong(&ok, 16))
				.arg(QDateTime::currentDateTime().toString("yyyyMMdd_hh:mm:ss:zzz"))
				.arg(info.current.source)
				.arg(motRegs)
				.arg(info.motionAlarmThrPercent)
				.arg(info.ios.at(0))
				.arg(info.current.id.toLongLong(&ok, 16))
				.arg(info.current.source)
				.arg(motRegs)
				.arg(info.motionAlarmThrPercent)
				.arg(info.ios.at(0))
				.arg(motRegs)
				.arg(sign.constData())

				.toUtf8();
	}

	return ba;
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
	if (info.algorithm == 0)
		setSeiField(buf);
	else if (info.algorithm == 1)
		((RawBuffer *)&buf)->pars()->metaData = generateAlarm(buf);

	/* we are done */
	lock.unlock();

	return newOutputBuffer(0, buf);
}

void SeiInserter::setAlarmID()
{
	seiAlarm.alarmNumber = alarmID;
	alarmID++;
}
