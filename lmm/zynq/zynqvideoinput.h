#ifndef ZYNQVIDEOINPUT_H
#define ZYNQVIDEOINPUT_H

#include <lmm/v4l2input.h>

class ZynqVideoInput : public V4l2Input
{
	Q_OBJECT
public:
	explicit ZynqVideoInput(QObject *parent = 0);
	int printMediaInfo(const QString &filename);
	QSize getSize(int ch);
signals:

public slots:
protected:
	virtual int openCamera();
	virtual int closeCamera();
private:

	uint pixFmt;
};

#endif // ZYNQVIDEOINPUT_H
