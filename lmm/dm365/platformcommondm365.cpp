#include "platformcommondm365.h"
#include "dmai/dmaiencoder.h"

#include <ti/sdo/ce/osal/SemMP.h>
#include "dm365/vicp.h"

static void sdk_hack() __attribute__ ((used));

PlatformCommonDM365::PlatformCommonDM365()
{
}

static void sdk_hack()
{
	SemMP_pend(NULL, 0);
	VICP_wait(VICP_HDVICP1);
}

void PlatformCommonDM365::platformInit()
{
	DmaiEncoder::initCodecEngine();
}

void PlatformCommonDM365::platformCleanUp()
{
}
