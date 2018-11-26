#include "functionpipeelement.h"

FunctionPipeElement::FunctionPipeElement(FunctionPipeElement::processFunc func, QObject *parent)
	: BaseLmmElement(parent)
{
	ftarget = func;
	f2target = NULL;
	otarget = NULL;
}

FunctionPipeElement::FunctionPipeElement(FunctionPipeElement::processFunc2 func, QObject *parent)
	: BaseLmmElement(parent)
{
	ftarget = NULL;
	f2target = func;
	otarget = NULL;
}

FunctionPipeElement::FunctionPipeElement(PipeElementoid *eoid, QObject *parent)
	: BaseLmmElement(parent)
{
	ftarget = NULL;
	f2target = NULL;
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

	if (f2target) {
		RawBuffer buf2 = (*f2target)(buf);
		return newOutputBuffer(buf2);
	}

	int err = otarget->processBuffer(buf);
	if (err)
		return err;

	return newOutputBuffer(buf);
}
