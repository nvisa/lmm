#ifndef THREADSAFELIST_H
#define THREADSAFELIST_H

#include <QList>
#include <QMutex>
#include <QObject>
#include <QMutexLocker>

template <class T>
class ThreadSafeList
{
public:
	explicit ThreadSafeList()
	{
	}
	void append(const T &t)
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		l.append(t);
	}
	void prepend(const T &t)
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		l.prepend(t);
	}
	void insert(int i, const T &t)
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		l.insert(i, t);
	}
	T takeAt(int i)
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		return l.takeAt(i);
	}
	T takeFirst()
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		return l.takeFirst();
	}
	T takeLast()
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		l.takeLast();
	}
	int count(const T &t)
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		return l.count(t);
	}
	int count()
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		return l.count();
	}
	int size()
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		return l.size();
	}
	T last()
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		return l.last();
	}

	const T &operator[](int i) const
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		return l[i];
	}

	T &operator[](int i)
	{
		QMutexLocker locker(&lock); Q_UNUSED(locker);
		return l[i];
	}

protected:
	QList<T> l;
	QMutex lock;
};

#endif // THREADSAFELIST_H
