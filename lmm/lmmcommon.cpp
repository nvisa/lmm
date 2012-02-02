#include "lmmcommon.h"
#include "dmaidecoder.h"
#include "emdesk/hardwareoperations.h"
#include "emdesk/debug.h"

#include <signal.h>

#include <QThread>

static void signalHandler(int signalNumber)
{
	qWarning("main: Received signal %d, thread id is %p", signalNumber, QThread::currentThreadId());
	if (signalNumber == SIGSEGV) {
		DmaiDecoder::cleanUpDsp();
		exit(0);
	} else if (signalNumber == SIGINT) {
		DmaiDecoder::cleanUpDsp();
		exit(0);
	} else if (signalNumber == SIGTERM) {
		DmaiDecoder::cleanUpDsp();
		exit(0);
	}
}

LmmCommon::LmmCommon(QObject *parent) :
	QObject(parent)
{
}

int LmmCommon::init()
{
	initDebug();
	HardwareOperations::writeRegister(0x1c7260c, 0x3004);
	DmaiDecoder::initCodecEngine();
	return 0;
}

int LmmCommon::installSignalHandlers()
{
	struct sigaction sigInstaller;
	sigset_t block_mask;

	sigemptyset(&block_mask);
	sigfillset(&block_mask);
	sigInstaller.sa_flags   = (SA_RESTART) ;
	sigInstaller.sa_mask    = block_mask;
	sigInstaller.sa_handler = &signalHandler;
	sigaction(SIGSEGV, &sigInstaller, NULL);
	sigaction(SIGINT, &sigInstaller, NULL);
	sigaction(SIGTERM, &sigInstaller, NULL);
	return 0;
}
