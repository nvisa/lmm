#ifndef LMSDEMO_H
#define LMSDEMO_H

#include <QWidget>

namespace Ui {
class LmsDemo;
}

class LmsDemo : public QWidget
{
	Q_OBJECT
	
public:
	explicit LmsDemo(QWidget *parent = 0);
	~LmsDemo();
	
private:
	Ui::LmsDemo *ui;
};

#endif // LMSDEMO_H
