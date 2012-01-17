#ifndef LMSDEMO_H
#define LMSDEMO_H

#include <QWidget>

namespace Ui {
class LmsDemo;
}

class AviDecoder;
class QTimer;

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

private:
	Ui::LmsDemo *ui;
	AviDecoder *dec;
	QTimer *timer;
	bool enableSliderUpdate;
	int hideCounter;
	int labelHideCounter;

	void updateVirtPosition(int val);
};

#endif // LMSDEMO_H
