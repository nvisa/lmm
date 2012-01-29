#ifndef STREAMTIME_H
#define STREAMTIME_H

#include <QObject>

class QTime;

class StreamTime : public QObject
{
	Q_OBJECT
public:
	explicit StreamTime(QObject *parent = 0);
	void setCurrentTime(qint64 val);
	void incrementCurrentTime(qint64 val) { currentTime += val; }
	qint64 getCurrentTime();
	void setStartTime(qint64 val) { startTime = val; }
	qint64 getStartPts() { return startPts; }
	void setStartPts(qint64 val) { startPts = val; }
	qint64 getStartTime() { return startTime; }
	void start();
	void stop();
	qint64 getFreeRunningTime();
signals:
	
public slots:
private:
	qint64 currentTime;
	qint64 startTime;
	qint64 startPts;
	QTime *drifter;
	QTime *clock;
};

#endif // STREAMTIME_H
