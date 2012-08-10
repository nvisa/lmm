#ifndef LMMCOMMON_H
#define LMMCOMMON_H

#include <QObject>

namespace Lmm {
	enum VideoOutput {
		COMPOSITE,
		COMPONENT
	};
}

class QGraphicsView;
class BaseLmmPlayer;
class BaseLmmElement;
class LmmCommon : public QObject
{
	Q_OBJECT
public:
	explicit LmmCommon(QObject *parent = 0);
	static int init();
	static int installSignalHandlers();
	static int registerForPipeSignal(BaseLmmElement *el);
	static QString getLibraryVersion();
	static QString getLiveMediaVersion();
	static QString getLibVlcVersion();
#ifdef CONFIG_DM6446
	static int showDecodeInfo(QGraphicsView *view, BaseLmmPlayer *dec);
#endif
signals:
	
public slots:
	
};

#endif // LMMCOMMON_H
