#ifndef TOKENBUCKET_H
#define TOKENBUCKET_H

#include <QMutex>
#include <QElapsedTimer>
#include <QObject>

class QTimer;

class TokenBucket : public QObject
{
	Q_OBJECT
public:
	explicit TokenBucket(QObject *parent = 0);

	void checkTime();
	int get(int count);
	void setPars(int averageRate, int burstRate, int burstDuration = 50);
	void setParsLeaky(int averageRate, int checkDuration = 50);

protected:
	int releaseLeakyTokens();

private:
	QMutex l;
	QElapsedTimer t;
	int tokens;
	bool leaky;

	/* parameters */
	int b;
	int M;
	int r;
};

#endif // TOKENBUCKET_H
