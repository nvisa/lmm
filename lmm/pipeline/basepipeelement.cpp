#include "basepipeelement.h"
#include "debug.h"
#include "baselmmpipeline.h"

#include <errno.h>

class RateReducto {
public:
	RateReducto()
	{
		enabled = false;
		skip = 0;
		total = 0;
		reset();
	}
	void reset()
	{
		skippedCount = 0;
		current = 0;
	}
	bool shouldSkip()
	{
		if (!enabled)
			return false;
		if (current++ < skip)
			return true;
		if (current == total)
			reset();
		return false;
	}

	int skip;
	int total;
	bool enabled;
	int skippedCount;
	int current;
};

BasePipeElement::BasePipeElement(BaseLmmPipeline *parent) :
	BaseLmmElement(parent),
	pipeline(parent)
{
	link.source = NULL;
	link.destination = NULL;
	link.reducto = new RateReducto;
	copyOnUse = false;
	outHook = NULL;
	outPriv = NULL;
}

int BasePipeElement::operationProcess()
{
	return link.source->processBlocking(link.sourceProcessChannel);
}

int BasePipeElement::operationBuffer()
{
	const RawBuffer buf = link.source->nextBufferBlocking(link.sourceChannel);
	if (!link.destination)
		return 0;
	if (outHook)
		outHook(buf, outPriv);
	foreach (const outLink &l, copyLinks)
		if (!l.reducto || !l.reducto->shouldSkip())
			l.destination->addBuffer(l.destinationChannel, buf);
	if (link.reducto->shouldSkip())
		return 0;
	return link.destination->addBuffer(link.destinationChannel, buf);
}

int BasePipeElement::addBuffer(int ch, const RawBuffer &buffer)
{
	if (passThru)
		return link.destination->addBuffer(link.destinationChannel, buffer);
	if (copyOnUse)
		return link.source->addBuffer(ch, buffer.makeCopy());
	return link.source->addBuffer(ch, buffer);
}

void BasePipeElement::setPipe(BaseLmmElement *target, BaseLmmElement *next, int tch, int pch, int nch)
{
	if (!target->objectName().isEmpty())
		setObjectName(QString("PipeOf%2").arg(target->objectName()));
	link.source = target;
	link.destination = next;
	link.sourceChannel = tch;
	link.sourceProcessChannel = pch;
	link.destinationChannel = nch;
}

void BasePipeElement::addNewLink(const outLink &link)
{
	copyLinks << link;
}

void BasePipeElement::addOutputHook(pipeHook hook, void *priv)
{
	outHook = hook;
	outPriv = priv;
}

int BasePipeElement::setRateReduction(int skip, int outOf)
{
	link.reducto->enabled = true;
	link.reducto->skip = skip;
	link.reducto->total = outOf;
	if (link.reducto->skip == 0)
		link.reducto->enabled = false;
	link.reducto->reset();
	return 0;
}

void BasePipeElement::threadFinished(LmmThread *thread)
{
	pipeline->threadFinished(thread);
}

int BasePipeElement::processBuffer(const RawBuffer &buf)
{
	Q_UNUSED(buf);
	/* this function will never be called */
	return -EINVAL;
}


int BasePipeElement::outLink::setRateReduction(int skip, int outOf)
{
	reducto->enabled = true;
	reducto->skip = skip;
	reducto->total = outOf;
	if (reducto->skip == 0)
		reducto->enabled = false;
	reducto->reset();
	return 0;
}
