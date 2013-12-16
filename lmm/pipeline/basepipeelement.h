#ifndef BASEPIPEELEMENT_H
#define BASEPIPEELEMENT_H

#include <lmm/baselmmelement.h>

class BasePipe
{
public:
	BasePipe(QObject *parent)
	{
		this->parent = parent;
	}
	virtual RawBuffer process(RawBuffer buf) = 0;
protected:
	QObject *parent;
};

template <class T>
class FunctionPipeElement : public BasePipe
{
public:
	typedef RawBuffer (T::*pipeOp)(RawBuffer);
	explicit FunctionPipeElement(T *targetClass, pipeOp op, QObject *parent = 0)
		: BasePipe(parent)
	{
		this->targetClass = targetClass;
		this->op = op;
	}
	RawBuffer process(RawBuffer buf)
	{
		return (targetClass->*op)(buf);
	}

protected:
	T *targetClass;
	pipeOp op;

};

class BasePipeElement : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BasePipeElement(QObject *parent = 0);

	int operationProcess();
	int operationBuffer();

	void setPipe(BaseLmmElement *target, BaseLmmElement *next = NULL, int tch = 0, int nch = 0);
	int setPipe(BasePipe *pipe);
	void setNextChannel(int ch) { nextCh = ch; }
	void setNext(BaseLmmElement *el) { next = el; }

protected:
	int processBuffer(RawBuffer buf);

	BaseLmmElement *target;
	BaseLmmElement *next;
	int targetCh;
	int nextCh;
	BasePipe *pipe;
	
};

#endif // BASEPIPEELEMENT_H
