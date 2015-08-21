#ifndef ERRORINJECTOR_H
#define ERRORINJECTOR_H

#include <QObject>

#include <lmm/tools/threadsafelist.h>

typedef int (*errorInjectFunc)(void *);

#ifdef ERROR_INJECTION_ENABLED
#define EI_ERR_POINT_(__x, __y, __px, __callback, __callback_arg) \
	{ \
		static int err_id_##__x##__y = -1; \
		static int once_##__x##__y = 0; \
		if (!once_##__x##__y) { \
			err_id_##__x##__y = ErrorInjector::instance()->addErrorPoint(__px, __y, __callback, __callback_arg); \
			once_##__x##__y = 1; \
		} else if (ErrorInjector::instance()->isEnabled(err_id_##__x##__y)) {\
			ErrorInjector::instance()->fire(err_id_##__x##__y); \
		} \
	}
#define EI_ERR_POINT(__callback, __callback_arg) EI_ERR_POINT_(__func__, __LINE__, __PRETTY_FUNCTION__, __callback, __callback_arg)
#else
#define EI_ERR_POINT(__x, __y)
#endif

class ErrorInfo;

class ErrorInjector : public QObject
{
	Q_OBJECT
public:
	static ErrorInjector * instance();
	int addErrorPoint(const QString &prettyFunction, int line, errorInjectFunc func, void *funcArg);
	int getErrorPointCount();
	QString getFunctionName(int index);
	QString getDescription(int index);
	int getLine(int index);
	bool isEnabled(int index);
	void setEnabled(int index, bool v);
	int fire(int index);
protected:
	explicit ErrorInjector(QObject *parent = 0);

	static ErrorInjector *inst;
	ThreadSafeList<ErrorInfo *> points;


};

#endif // ERRORINJECTOR_H
