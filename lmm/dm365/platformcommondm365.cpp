#include "platformcommondm365.h"
#include "dmaiencoder.h"

PlatformCommonDM365::PlatformCommonDM365()
{
}

void PlatformCommonDM365::platformInit()
{
	DmaiEncoder::initCodecEngine();
}

void PlatformCommonDM365::platformCleanUp()
{
}
