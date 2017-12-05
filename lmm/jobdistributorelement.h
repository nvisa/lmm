#ifndef JOBDISTRIBUTORELEMENT_H
#define JOBDISTRIBUTORELEMENT_H

#include <lmm/baselmmelement.h>

#include <QHash>

class ReduceThread;
class DistHandlerThread;

class JobDistributorElement : public BaseLmmElement
{
	Q_OBJECT
public:
	enum DistributionMethod {
		DIST_SEQUENTIAL,
		DIST_ALL,
	};

	JobDistributorElement(QObject *parent = 0);

	int addElement(BaseLmmElement *el);
	void setMethod(DistributionMethod m);
	int elementCount();

	int start();
	int stop();

	int finishBuffer(const RawBuffer &buf);

protected:
	virtual int processBuffer(const RawBuffer &buf);

	friend class ReduceThread;
	QList<DistHandlerThread *> free;
	ReduceThread *reducer;
	QHash<int, DistHandlerThread *> busy;
	QMutex dlock;
	DistributionMethod method;
};

#endif // JOBDISTRIBUTORELEMENT_H
