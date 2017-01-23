#ifndef SEIINSERTER_H
#define SEIINSERTER_H

#include <lmm/baselmmelement.h>
#include <lmm/interfaces/motiondetectioninterface.h>

#include <QMutex>
#include <QElapsedTimer>

class GpioController;

class SeiInserter : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit SeiInserter(QObject *parent = 0);

	void setMotionDetectionProvider(MotionDetectionInterface *iface);

	enum AlarmType {
		ALARM_STOP,
		ALARM_ONLINE,
		ALARM_POST,
		ALARM_IDLE
	};

	int setSeiProps(AlarmType type, const QByteArray &data);
	int addGpioAlarm(int io);
	int setAlarmInformation(const QString &templateFile, int alg, int gpioId, int ioLevel, int minDuration, int motThreshold);
	int setAlarmTemplate(const QString &filename);

protected:
	struct CurrentAlarmInfo {
		CurrentAlarmInfo()
		{
			active = false;
			source = -1;
			id = -1;
		}

		void triggerNew(int s)
		{
			source = s;
			active = true;
			id++;
			t.start();
		}

		void suppress()
		{
			active = false;
		}

		int active;
		QElapsedTimer t;
		int source;
		int id;
	};

	struct AlarmInfo {
		AlarmInfo()
		{
			algorithm = 0;
			enabled = false;
			motionAlarmThreshold = 0;
			minAlarmDuration = 0;
			ioAlarmLevel = 1;
			lastMotionValue = 0;
		}

		int algorithm;
		bool enabled;
		int motionAlarmThreshold;
		int ioAlarmLevel;
		QString alarmTemplate;
		QString alarmTemplateFilename;
		CurrentAlarmInfo current;
		int minAlarmDuration;
		QList<int> ios;
		int lastMotionValue;
	};
	AlarmInfo info;

	const QByteArray generateAlarm();
	int processBuffer(const RawBuffer &buf);

	/* SEI stuff */
	struct AlarmProps {
		AlarmType alarmType;
		int alarmNumber;
		int seiDuration;
		int alarmDuration;
		int seiOfftime;
	};
	void setAlarmID();
	int setSeiStatus();
	void setSeiField(RawBuffer buf);

	AlarmProps seiAlarm;
	int alarmID;
	bool insertSeiAlarm;
	int seiActDur;
	bool seiTimerStarted;
	bool isSeiNew;
	QByteArray incomingSeiData;
	AlarmType incomingAlarmType;
	QMutex lock;
	QElapsedTimer seiTimer;
	MotionDetectionInterface *motprov;
	GpioController *gpio;
};

#endif // SEIINSERTER_H
