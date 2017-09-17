#include "functionpipeelement.h"

FunctionPipeElement::FunctionPipeElement(FunctionPipeElement::processFunc func, QObject *parent)
	: BaseLmmElement(parent)
{
	ftarget = func;
	otarget = NULL;
}

FunctionPipeElement::FunctionPipeElement(PipeElementoid *eoid, QObject *parent)
	: BaseLmmElement(parent)
{
	ftarget = NULL;
	otarget = eoid;
}

int FunctionPipeElement::processBuffer(const RawBuffer &buf)
{
	if (ftarget) {
		int err = (*ftarget)(buf);
		if (err)
			return err;
		return newOutputBuffer(buf);
	}

	return otarget->processBuffer(buf);
}
