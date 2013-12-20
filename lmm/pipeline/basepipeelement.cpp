#include "basepipeelement.h"
#include "debug.h"
#include "baselmmpipeline.h"

#include <errno.h>

BasePipeElement::BasePipeElement(BaseLmmPipeline *parent) :
	BaseLmmElement(parent),
	pipeline(parent)
{
	pipe = NULL;
	target = NULL;
	next = NULL;
}

int BasePipeElement::operationProcess()
{
	if (pipe)
		return processBlocking(targetCh);
	return target->processBlocking(targetCh);
}

int BasePipeElement::operationBuffer()
{
	RawBuffer buf;
	if (target)
		buf = target->nextBufferBlocking(targetCh);
	else if (pipe)
		buf = nextBufferBlocking(targetCh);
	return next->addBuffer(nextCh, buf);
}

void BasePipeElement::setPipe(BaseLmmElement *target, BaseLmmElement *next, int tch, int nch)
{
	this->target = target;
	this->next = next;
	targetCh = tch;
	nextCh = nch;
}

int BasePipeElement::setPipe(BasePipe *pipe)
{
	this->pipe = pipe;
	targetCh = 0;
	nextCh = 0;
	next = NULL;
	return 0;
}

void BasePipeElement::threadFinished(LmmThread *thread)
{
	pipeline->threadFinished(thread);
}

int BasePipeElement::processBuffer(RawBuffer buf)
{
	if (!buf.isEOF())
		buf = pipe->process(buf);
	return newOutputBuffer(0, buf);
}
