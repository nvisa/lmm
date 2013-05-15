#ifndef LMMTHREAD_H
#define LMMTHREAD_H

#include <QThread>
#include <QSemaphore>

class LmmThread : public QThread
{
public:
	LmmThread(QString threadName);
	void stop();
	void pause();
	void resume();
	virtual void run();
	static void stopAll();
	QString threadName() { return name; }
protected:
	bool exit;
	bool paused;
	virtual int operation() = 0;
private:
	QString name;
	QSemaphore pauser;
};

#endif // LMMTHREAD_H
