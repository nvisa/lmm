#ifndef DM8168VIDEOCONTROLLER_H
#define DM8168VIDEOCONTROLLER_H

#include <QFile>
#include <QObject>

class DM8168VideoController : public QObject
{
	Q_OBJECT
public:
	explicit DM8168VideoController(QObject *parent = 0);
	void showVideo();
	void showGraphics();
	int useTransparencyKeying(bool enable, int key);
signals:
	
public slots:
private:
	QFile dispCtrlNode;
};

#endif // DM8168VIDEOCONTROLLER_H
