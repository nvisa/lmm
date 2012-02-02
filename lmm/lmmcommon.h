#ifndef LMMCOMMON_H
#define LMMCOMMON_H

#include <QObject>

class LmmCommon : public QObject
{
	Q_OBJECT
public:
	explicit LmmCommon(QObject *parent = 0);
	static int init();
	static int installSignalHandlers();
signals:
	
public slots:
	
};

#endif // LMMCOMMON_H
