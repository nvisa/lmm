#ifndef STREAMTIME_H
#define STREAMTIME_H

#include <QObject>

class StreamTime : public QObject
{
	Q_OBJECT
public:
	explicit StreamTime(QObject *parent = 0);
	void setCurrentTime(qint64 val) { currentTime = val; }
	void incrementCurrentTime(qint64 val) { currentTime += val; }
	qint64 getCurrentTime() { return currentTime; }
signals:
	
public slots:
private:
	qint64 currentTime;
};

#endif // STREAMTIME_H
