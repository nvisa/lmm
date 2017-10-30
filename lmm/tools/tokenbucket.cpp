#include "tokenbucket.h"

#include <lmm/debug.h>

#include <unistd.h>

TokenBucket::TokenBucket(QObject *parent) :
	QObject(parent)
{
	leaky = false;
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
	if (leaky) {
		while (b < count) {
			if (releaseLeakyTokens())
				break;
			usleep(1000);
		}
		l.lock();
		b -= count;
		tokens += count;
		l.unlock();
		return count;
	}
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
	leaky = false;
	if (burstDuration == 0)
		burstDuration = 50;
	float T = burstDuration / (float)1000;
	burstRate *= T;
	M = burstRate / T;
	r = averageRate;
	b = T * (M - r);
	tokens = b;
	t.start();
	mDebug("T = %f, M = %d, r = %d, b = %d", T, M, r, b);
}

void TokenBucket::setParsLeaky(int averageRate, int checkDuration)
{
	if (checkDuration == 0)
		checkDuration = 50;
	leaky = true;
	M = checkDuration;
	r = averageRate / (1000 / M);
	b = r; //holds the left data, i.e. 'n'
	t.start();
}

int TokenBucket::releaseLeakyTokens()
{
	if (t.elapsed() >= M) {
		l.lock();
		b = r;
		int br = tokens * 1000 / t.restart();
		if (br * 8 > 9000000)
			qDebug() << br;
		tokens = 0;
		l.unlock();
		return b;
	}
	return 0;
}
