#include "platformcommondm8168.h"

#include <lmm/debug.h>

#include <xdc/std.h>

#include <ti/ipc/Ipc.h>
#include <ti/ipc/MessageQ.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Core.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Component.h>

/* Constants for the program */
#define DSPSERVERNAME      "UIAConfigurationServerDsp"
#define VIDEOM3SERVERNAME  "UIAConfigurationServerVideoM3"
#define VPSSM3SERVERNAME   "UIAConfigurationServerVpssM3"

/* Diagnostics control masks*/
#define OMX_DEBUG_OFF    0
#define OMX_DEBUG_LEVEL1 1
#define OMX_DEBUG_LEVEL2 2
#define OMX_DEBUG_LEVEL3 4
#define OMX_DEBUG_LEVEL4 8
#define OMX_DEBUG_LEVEL5 16

#define OMX_MSGQ_SERVERNAME_STRLEN     (50)

#define UIA_CONFIGURE_CMD (0x55)
#define UIA_CONFIGURE_ACK (0x56)
#define NACK              (0xFF)

#define OMX_MSGHEAPID_CORE0              (2)
#define OMX_MSGHEAPID_CORE1              (4)
#define OMX_MSGHEAPID_CORE2              (5)

typedef struct ConfigureUIA
{
  MessageQ_MsgHeader header;
  Int enableAnalysisEvents;
  Int debugLevel;
  Int enableStatusLogger;
} ConfigureUIA;

static Void configureUiaLoggerClient (Int coreId, ConfigureUIA *pCfgUIA)
{
  MessageQ_Handle messageQ;
  MessageQ_QueueId serverQueue;
  MessageQ_Msg msg;
  Int status;
  Char serverName[OMX_MSGQ_SERVERNAME_STRLEN];
  Int heapId = 0;

  if (coreId == 0)
  {
	strcpy (serverName, DSPSERVERNAME);
	heapId = OMX_MSGHEAPID_CORE0;
  }
  else if (coreId == 1)
  {
	strcpy (serverName, VIDEOM3SERVERNAME);
	heapId = OMX_MSGHEAPID_CORE1;
  }
  else if (coreId == 2)
  {
	strcpy (serverName, VPSSM3SERVERNAME);
	heapId = OMX_MSGHEAPID_CORE2;
  }
  else
  {
	printf ("Invalid Core id - should be 1 or 2");
	return;
  }

  /*
   *  Create client's MessageQ.
   *
   *  Use 'NULL' for name since no since this queueId is passed by
   *  referene and no one opens it by name.
   *  Use 'NULL' for params to get default parameters.
   */
  messageQ = MessageQ_create (NULL, NULL);
  if (messageQ == NULL)
  {
	fDebug("Failed to create MessageQ\n");
	return;
  }

  /* Open the server's MessageQ */
  do
  {
	status = MessageQ_open (serverName, &serverQueue);
  } while (status < 0);

  msg = MessageQ_alloc (heapId, sizeof (ConfigureUIA));
  if (msg == NULL)
  {
	fDebug ("MessageQ_alloc failed\n");
	return;
  }

  /* Have the remote processor reply to this message queue */
  MessageQ_setReplyQueue (messageQ, msg);

  /* Loop requesting information from the server task */
  printf ("UIAClient is ready to send a UIA configuration command\n");

  /* Server will increment and send back */
  MessageQ_setMsgId (msg, UIA_CONFIGURE_CMD);

  ((ConfigureUIA *) msg)->enableStatusLogger = pCfgUIA->enableStatusLogger;
  ((ConfigureUIA *) msg)->debugLevel = pCfgUIA->debugLevel;
  ((ConfigureUIA *) msg)->enableAnalysisEvents = pCfgUIA->enableAnalysisEvents;

  /* Send the message off */
  status = MessageQ_put (serverQueue, msg);
  if (status < 0)
  {
	MessageQ_free (msg);
	fDebug ("MessageQ_put failed\n");
	return;
  }

  /* Wait for the reply... */
  status = MessageQ_get (messageQ, &msg, MessageQ_FOREVER);
  if (status < 0)
  {
	fDebug ("MessageQ_get had an error\n");
	return;
  }

  /* Validate the returned message. */
  if (MessageQ_getMsgId (msg) != UIA_CONFIGURE_ACK)
  {
	fDebug ("Unexpected value\n");
	return;
  }

  fDebug ("UIAClient received UIA_CONFIGURE_ACK\n");

  MessageQ_free(msg);

  status = MessageQ_close (&serverQueue);
  if (status < 0)
  {
	fDebug ("MessageQ_close failed\n");
	return;
  }

  status = MessageQ_delete (&messageQ);
  if (status < 0)
  {
	fDebug ("MessageQ_delete failed\n");
	return;
  }

  fDebug ("UIAClient is done sending requests\n");
}

PlatformCommonDM8168::PlatformCommonDM8168()
{
}

void PlatformCommonDM8168::platformInit()
{
	OMX_ERRORTYPE err = OMX_Init();
	fDebug("OMX core inited with err %d", int(err));

	ConfigureUIA uiaCfg;
	/* Configuring logging options on slave cores */
	/* can be 0 or 1 */
	uiaCfg.enableAnalysisEvents = 0;
	/* can be 0 or 1 */
	uiaCfg.enableStatusLogger = 1;
	/* can be OMX_DEBUG_LEVEL1|2|3|4|5 */
	uiaCfg.debugLevel = OMX_DEBUG_LEVEL2;
	/* configureUiaLoggerClient( COREID, &Cfg); */
	//configureUiaLoggerClient(2, &uiaCfg);
	//configureUiaLoggerClient(1, &uiaCfg);
}

void PlatformCommonDM8168::platformCleanUp()
{
	OMX_ERRORTYPE err = OMX_Deinit();
	fDebug("OMX core de-inited with err %d", int(err));
}
