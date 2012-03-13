#ifndef LMMCOMMON_H
#define LMMCOMMON_H

#include <QObject>

class QGraphicsView;
class BaseLmmPlayer;
class LmmCommon : public QObject
{
	Q_OBJECT
public:
	explicit LmmCommon(QObject *parent = 0);
	static int init();
	static int installSignalHandlers();
#ifdef CONFIG_DM6446
	static int showDecodeInfo(QGraphicsView *view, BaseLmmPlayer *dec);
#endif
signals:
	
public slots:
	
};

#endif // LMMCOMMON_H
