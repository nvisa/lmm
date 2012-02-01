#ifndef LMSDEMO_H
#define LMSDEMO_H

#include <QWidget>
#include <QMap>

namespace Ui {
class LmsDemo;
}

class AviPlayer;
class QTimer;
class QGraphicsScene;
class BaseLmmElement;
class BaseLmmPlayer;
class QGraphicsSimpleTextItem;

class LmsDemo : public QWidget
{
	Q_OBJECT
	
public:
	explicit LmsDemo(QWidget *parent = 0);
	void exitLater();
	~LmsDemo();
private slots:
	void timeout();
	void cleanUpAndExit();
	void on_toolPlay_clicked();

	void on_toolStop_clicked();

	void on_toolPrevPage_clicked();
	void on_toolForward_clicked();

	void on_toolBackward_clicked();

	void on_toolMoviePage_clicked();

	void on_toolMute_clicked();

protected:
	bool eventFilter(QObject *, QEvent *);
private:
	Ui::LmsDemo *ui;
	BaseLmmPlayer *dec;
	QTimer *timer;
	QGraphicsScene *scene;
	QMap<BaseLmmElement *, QList<QGraphicsSimpleTextItem *> > visuals;
	bool enableSliderUpdate;
	int hideCounter;
	int labelHideCounter;

	void updateVirtPosition(int val);
	void showDecodeInfo();
	void addElement(BaseLmmElement *el);
};

#endif // LMSDEMO_H
