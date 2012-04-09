#ifndef LMMTHREAD_H
#define LMMTHREAD_H

#include <QThread>

class LmmThread : public QThread
{
public:
	LmmThread(QString threadName);
	void stop();
	virtual void run();
protected:
	bool exit;
	virtual int operation() = 0;
private:
	QString name;
};

#endif // LMMTHREAD_H
