#include "errorinjector.h"
#include "debug.h"

ErrorInjector * ErrorInjector::inst = NULL;

class ErrorInfo
{
public:
	ErrorInfo()
	{
		enabled = false;
		index = -1;
	}

	QString funcName;
	int line;
	bool enabled;
	QString description;
	int index;
	errorInjectFunc func;
	void *funcArg;
};

ErrorInjector * ErrorInjector::instance()
{
	if (!inst)
		inst = new ErrorInjector;
	return inst;
}

int ErrorInjector::addErrorPoint(const QString &prettyFunction, int line, errorInjectFunc func, void *funcArg)
{
	mDebug("%s : %d", qPrintable(prettyFunction), line);
	ErrorInfo *info = new ErrorInfo;
	info->funcName = prettyFunction;
	info->line = line;
	info->index = points.size();
	info->func = func;
	info->funcArg = funcArg;
	points.append(info);
	return info->index;
}

int ErrorInjector::getErrorPointCount()
{
	return points.size();
}

QString ErrorInjector::getFunctionName(int index)
{
	return points[index]->funcName;
}

QString ErrorInjector::getDescription(int index)
{
	return points[index]->description;
}

int ErrorInjector::getLine(int index)
{
	return points[index]->line;
}

bool ErrorInjector::isEnabled(int index)
{
	return points[index]->enabled;
}

void ErrorInjector::setEnabled(int index, bool v)
{
	points[index]->enabled = v;
}

int ErrorInjector::fire(int index)
{
	const ErrorInfo *info = points[index];
	return (info->func)(info->funcArg);
}

ErrorInjector::ErrorInjector(QObject *parent) :
	QObject(parent)
{
}
