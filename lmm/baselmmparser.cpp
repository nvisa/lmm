#include "baselmmparser.h"

#include <lmm/debug.h>

#include <errno.h>

BaseLmmParser::BaseLmmParser(QObject *parent) :
	BaseLmmElement(parent)
{
}

int BaseLmmParser::processBuffer(const RawBuffer &buf)
{
	return 0;
}
