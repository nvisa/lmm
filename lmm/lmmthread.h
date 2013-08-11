#ifndef LMMTHREAD_H
#define LMMTHREAD_H

#include <QTime>
#include <QMutex>
#include <QThread>
#include <QSemaphore>

class LmmThread : public QThread
{
public:
	enum Status {
		IN_OPERATION,
		PAUSED,
		QUIT
	};

	LmmThread(QString threadName);
	void stop();
	void pause();
	void resume();
	virtual void run();
	static void stopAll();
	static LmmThread * getById(Qt::HANDLE id);
	QString threadName() { return name; }
	Status getStatus();
	int elapsed();
protected:
	bool exit;
	bool paused;
	virtual int operation() = 0;
private:
	QString name;
	QSemaphore pauser;
	Status st;
	QTime time;
	QMutex lock;
	Qt::HANDLE id;
};

#endif // LMMTHREAD_H
