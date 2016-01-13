#ifndef SEIINSERTER_H
#define SEIINSERTER_H

#include <lmm/baselmmelement.h>

#include <QMutex>
#include <QElapsedTimer>

class SeiInserter : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit SeiInserter(QObject *parent = 0);

	enum AlarmType {
		ALARM_STOP,
		ALARM_ONLINE,
		ALARM_POST,
		ALARM_IDLE
	};

	int setSeiProps(AlarmType type, const QByteArray &data);
	int setSetting(const QString &setting, const QVariant &value);
	QVariant getSetting(const QString &setting);

protected:
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
};

#endif // SEIINSERTER_H
