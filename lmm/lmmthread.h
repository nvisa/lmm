#ifndef LMMTHREAD_H
#define LMMTHREAD_H

#include <QMutex>
#include <QThread>
#include <QElapsedTimer>

#include <stdint.h>

class UnitTimeStat;
class BaseLmmElement;

class LmmThread : public QThread
{
public:
	enum Status {
		IN_OPERATION,
		PAUSED,
		QUIT
	};

	LmmThread(QString threadName, BaseLmmElement *parent = 0);
	void stop();
	void pause();
	void resume();
	virtual void run();
	static void stopAll();
	static LmmThread * getById(Qt::HANDLE id);
	QString threadName() { return name; }
	Status getStatus();
	int elapsed();
	int getAverageRunTime();
	void printStack();
protected:
	bool exit;
	bool paused;
	virtual int operation() = 0;
private:
	QString name;
	Status st;
	QElapsedTimer time;
	QMutex lock;
	Qt::HANDLE id;
	intptr_t *instStack1;
	intptr_t *instStack2;
	intptr_t *instPos;
	BaseLmmElement *parent;
	UnitTimeStat *opTimeStat;
};

#endif // LMMTHREAD_H
