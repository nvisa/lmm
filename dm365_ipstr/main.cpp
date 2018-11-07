#include <QSettings>
#include <QStringList>
#include <QCoreApplication>

#include <lmm/version.h>
#include <lmm/lmmthread.h>
#include <lmm/dm365/cvbsstreamer.h>
#include <lmm/dm365/ipcamerastreamer.h>
#include <lmm/dm365/simplertpstreamer.h>
#include <lmm/dm365/simple1080pstreamer.h>

#include <ecl/debug.h>
#include <ecl/net/remotecontrol.h>
#include <ecl/settings/applicationsettings.h>

#include "genericstreamer.h"
#include "jpegstreamer.h"

#include <signal.h>
#include <execinfo.h>

static void printStackTrace(void)
{
	void *array[25];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace (array, 25);
	strings = backtrace_symbols (array, size);

	qWarning("Obtained %zd stack frames.", size);

	for (i = 0; i < size; i++)
		qWarning("%s", strings[i]);

	free (strings);
}

static void signalHandler(int signalNumber)
{
	static int once = 0;
	if (once) {
		exit(23);
		return;
	}
	once = 1;

	Qt::HANDLE id = QThread::currentThreadId();
	LmmThread *t = LmmThread::getById(id);
	qWarning("crash: Received signal %d, thread id is %p(%s)",
			 signalNumber, id, t ? qPrintable(t->threadName()) : "main");
	printStackTrace();

	exit(19);
}

static int installSignalHandlers()
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
	sigaction(SIGABRT, &sigInstaller, NULL);
	sigaction(SIGPIPE, &sigInstaller, NULL);
	sigaction(SIGBUS, &sigInstaller, NULL);
	sigaction(SIGFPE, &sigInstaller, NULL);
	return 0;
}

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	if (a.arguments().contains("--version")) {
		qDebug() << VERSION_INFO;
		return 0;
	}

	bool onvifEnabled = false;
	QSettings amansets("/etc/appmanager.conf", QSettings::IniFormat);
	amansets.beginGroup("Applications");
	int size = amansets.beginReadArray("apps");
	for (int i = 0; i < size; i++) {
		amansets.setArrayIndex(i);
		QString appPath = amansets.value("app_path").toString();
		QString appRunMode = amansets.value("app_run_mode").toString();
		if (appPath.endsWith("onvif_nvt") && appRunMode != "manual")
			onvifEnabled = true;
	}

	installSignalHandlers();

	LmmCommon::init();
	ecl::initDebug();
	ApplicationSettings *sets = ApplicationSettings::instance();
	if (onvifEnabled)
		sets->load("/etc/encsoft/dm365_ipstr_onvif.json", QIODevice::ReadOnly);
	else
		sets->load("/etc/encsoft/dm365_ipstr.json", QIODevice::ReadOnly);

	if (sets->get("config.remote_control.enabled").toBool()) {
		fDebug("starting remote control");
		RemoteControl *r = new RemoteControl(&a);
		r->listen(QHostAddress::Any, sets->get("config.remote_control.port").toInt());
	}

	BaseStreamer *s = NULL;
	if (a.arguments().contains("--1080p"))
		s = new Simple1080pStreamer;
	else if (a.arguments().contains("--cvbs"))
		s = new CVBSStreamer;
	else if (a.arguments().contains("--jpeg")) {
		int index = a.arguments().indexOf("--jpeg");
		int bufferCount = 10;
		int flags = 0;
		if (a.arguments().size() > index + 1)
			bufferCount = a.arguments()[index + 1].toInt();
		index = a.arguments().indexOf("--flags");
		if (a.arguments().size() > index + 1)
			flags = a.arguments()[index + 1].toInt();
		s = new JpegStreamer(bufferCount, flags);
	} else
		s = new GenericStreamer;
	s->start();

	return a.exec();
}
