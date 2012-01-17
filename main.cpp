#include <QtGui/QApplication>
#include <QThread>

#include "lmsdemo.h"
#include "lmm/dmaidecoder.h"
#include "emdesk/hardwareoperations.h"

#include <signal.h>

static LmsDemo *w;

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

static void installSignalHandlers()
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
}

int main(int argc, char *argv[])
{
	HardwareOperations::writeRegister(0x1c7260c, 0x3004);
	DmaiDecoder::initCodecEngine();
	installSignalHandlers();
	QApplication a(argc, argv);
	w = new LmsDemo;
	w->show();
	
	return a.exec();
}
