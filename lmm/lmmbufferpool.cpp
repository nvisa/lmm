#include "lmmbufferpool.h"

LmmBufferPool::LmmBufferPool(QObject *parent) :
	QObject(parent)
{
}

LmmBufferPool::LmmBufferPool(int count, QObject *parent)
{
}

void LmmBufferPool::addBuffer(RawBuffer buf)
{
}

int LmmBufferPool::totalBufferCount()
{
}

int LmmBufferPool::freeBufferCount()
{
}

RawBuffer LmmBufferPool::take()
{
}

int LmmBufferPool::give(RawBuffer buf)
{
}
