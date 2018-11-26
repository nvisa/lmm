#ifndef FUNCTIONPIPEELEMENT_H
#define FUNCTIONPIPEELEMENT_H

#include <lmm/baselmmelement.h>

class PipeElementoid
{
public:
	virtual int processBuffer(const RawBuffer &buf) = 0;
	virtual ~PipeElementoid() = 0;
};

template<class T>
class GenericPipeElementoid : public PipeElementoid
{
public:
	typedef int (T::*pipeOp)(const RawBuffer &buf);

	GenericPipeElementoid(T *c, pipeOp member)
	{
		this->c = c;
		func = member;
	}

	int processBuffer(const RawBuffer &buf)
	{
		return (c->*func)(buf);
	}

protected:
	T *c;
	pipeOp func;
};

inline PipeElementoid::~PipeElementoid() {}

class FunctionPipeElement : public BaseLmmElement
{
	Q_OBJECT
public:
	typedef int (*processFunc)(const RawBuffer &buf);
	typedef RawBuffer (*processFunc2)(const RawBuffer &buf);

	FunctionPipeElement(processFunc func, QObject *parent = NULL);
	FunctionPipeElement(processFunc2 func, QObject *parent = NULL);
	FunctionPipeElement(PipeElementoid *eoid, QObject *parent = NULL);

protected:
	virtual int processBuffer(const RawBuffer &buf);

	processFunc ftarget;
	processFunc2 f2target;
	PipeElementoid *otarget;
};

#define newFunctionPipe(_class, _inst, _func) \
	new FunctionPipeElement(new GenericPipeElementoid<_class>(_inst, &_func))

#endif // FUNCTIONPIPEELEMENT_H
