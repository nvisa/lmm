#ifndef NEONCOLORSPACE_H
#define NEONCOLORSPACE_H

#include <lmm/baselmmelement.h>

class NeonColorSpace : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit NeonColorSpace(QObject *parent = 0);

protected:
	int processBuffer(const RawBuffer &buf);

};

#endif // NEONCOLORSPACE_H
