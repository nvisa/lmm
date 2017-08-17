#ifndef UVCVIDEOINPUT_H
#define UVCVIDEOINPUT_H

#include <QObject>

#include <lmm/v4l2input.h>

class UvcVideoInput : public V4l2Input
{
	Q_OBJECT
public:
	UvcVideoInput(QObject *parent = NULL);

protected:
	virtual int openCamera();
};

#endif // UVCVIDEOINPUT_H
