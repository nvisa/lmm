#include "basepipeelement.h"
#include "debug.h"
#include "baselmmpipeline.h"

#include <errno.h>

BasePipeElement::BasePipeElement(BaseLmmPipeline *parent) :
	BaseLmmElement(parent),
	pipeline(parent)
{
	link.source = NULL;
	link.destination = NULL;
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
		l.destination->addBuffer(l.destinationChannel, buf);
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
