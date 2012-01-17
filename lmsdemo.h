#ifndef LMSDEMO_H
#define LMSDEMO_H

#include <QWidget>

namespace Ui {
class LmsDemo;
}

class AviDecoder;

class LmsDemo : public QWidget
{
	Q_OBJECT
	
public:
	explicit LmsDemo(QWidget *parent = 0);
	void exitLater();
	~LmsDemo();
private slots:
	void cleanUpAndExit();
	void on_toolPlay_clicked();

	void on_toolStop_clicked();

	void on_toolPrevPage_clicked();

private:
	Ui::LmsDemo *ui;
	AviDecoder *dec;
};

#endif // LMSDEMO_H
