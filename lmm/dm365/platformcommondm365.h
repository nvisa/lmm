#ifndef PLATFORMCOMMONDM365_H
#define PLATFORMCOMMONDM365_H

#include "lmmcommon.h"

class PlatformCommonDM365 : public PlatformCommon
{
public:
	PlatformCommonDM365();
	virtual void platformInit();
	virtual void platformCleanUp();
};

#endif // PLATFORMCOMMONDM365_H
