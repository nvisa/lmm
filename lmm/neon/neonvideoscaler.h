#ifndef NEONVIDEOSCALER_H
#define NEONVIDEOSCALER_H

#include <lmm/baselmmelement.h>

class NeonVideoScaler : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit NeonVideoScaler(QObject *parent = 0);

protected:
	int processBuffer(const RawBuffer &buf);

};

#endif // NEONVIDEOSCALER_H
