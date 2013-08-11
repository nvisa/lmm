#ifndef PLATFORMCOMMONDM8168_H
#define PLATFORMCOMMONDM8168_H

#include <lmm/lmmcommon.h>

class PlatformCommonDM8168 : public PlatformCommon
{
public:
	PlatformCommonDM8168();
	virtual void platformInit();
	virtual void platformCleanUp();
};

#endif // PLATFORMCOMMONDM8168_H
