#include "tokenbucket.h"

#include <lmm/debug.h>

TokenBucket::TokenBucket(QObject *parent) :
	QObject(parent)
{
}

void TokenBucket::checkTime()
{
	l.lock();
	int S = t.restart();
	if (S < 0)
		S = 0;
	tokens += (r * S) / 1000;
	/* tokens can be negative due to integer overflow */
	if (tokens > b || tokens < 0)
		tokens = b;
	l.unlock();
}

int TokenBucket::get(int count)
{
	checkTime();
	l.lock();
	while (tokens < count) {
		l.unlock();
		usleep(10000);
		checkTime();
		l.lock();
	}
	tokens -= count;
	l.unlock();
	return count;
}

void TokenBucket::setPars(int averageRate, int burstRate, int burstDuration)
{
	float T = burstDuration / (float)1000;
	burstRate *= T;
	M = burstRate / T;
	r = averageRate;
	b = T * (M - r);
	tokens = b;
	t.start();
	mDebug("T = %f, M = %d, r = %d, b = %d", T, M, r, b);
}
