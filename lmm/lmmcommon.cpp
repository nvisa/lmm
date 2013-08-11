#define __STDC_CONSTANT_MACROS
#include "lmmcommon.h"
#include "version.h"
#ifdef CONFIG_DM6446
#include "dm6446/platformcommondm6446.h"
#endif
#ifdef CONFIG_DM365
#include "dm365/platformcommondm365.h"
#endif
#ifdef CONFIG_DM8168
#include "dm8168/platformcommondm8168.h"
#endif
#include "lmmthread.h"
#ifdef CONFIG_FFMPEG
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
#endif
#include "baselmmelement.h"
#include "hardwareoperations.h"
#include "debug.h"

#include <signal.h>

#include <QMap>
#include <QThread>
#include <QThreadPool>

#ifdef USE_GSTREAMER
#include <gst/gst.h>
#endif

LmmCommon LmmCommon::inst;

int dbgtemp = 0;
static 	bool quitOnSigInt = true;
static QList<BaseLmmElement *> registeredElementsForPipe;
static QMap<int, QList<BaseLmmElement *> > registeredElements;

void LmmCommon::platformCleanUp()
{
	if (inst.plat)
		inst.plat->platformCleanUp();
}

static QMap<int, int> signalCount;

void LmmCommon::platformInit()
{
	if (inst.plat)
		inst.plat->platformInit();
}

void notifyRegistrars(int signal)
{
	const QList<BaseLmmElement *> list = registeredElements[signal];
	foreach (BaseLmmElement *el, list)
		el->signalReceived(signal);
}

static void signalHandler(int signalNumber)
{
	signalCount[signalNumber] += 1;
	Qt::HANDLE id = QThread::currentThreadId();
	if (signalCount[signalNumber] == 1) {
		LmmThread *t = LmmThread::getById(id);
		qWarning("main: Received signal %d, thread id is %p(%s), dbgtemp is %d",
				 signalNumber, id, t ? qPrintable(t->threadName()) : "main", dbgtemp);
	}
	if (signalNumber == SIGSEGV) {
		LmmThread::stopAll();
		LmmCommon::platformCleanUp();
		exit(0);
	} else if (signalNumber == SIGINT) {
		if (quitOnSigInt) {
			LmmThread::stopAll();
			LmmCommon::platformCleanUp();
			qDebug("sigint clean-up done, exiting");
			exit(0);
		} else
			qDebug("%s: sigint is not active", __func__);
	} else if (signalNumber == SIGTERM) {
		LmmThread::stopAll();
		LmmCommon::platformCleanUp();
		exit(0);
	} else if (signalNumber == SIGABRT) {
		LmmThread::stopAll();
		LmmCommon::platformCleanUp();
		exit(0);
	} else if (signalNumber == SIGPIPE) {
		foreach (BaseLmmElement *el, registeredElementsForPipe)
			el->signalReceived(signalNumber);
	} else {
		LmmThread::stopAll();
		LmmCommon::platformCleanUp();
		exit(0);
	}
	notifyRegistrars(signalNumber);
}

LmmCommon::LmmCommon(QObject *parent) :
	QObject(parent)
{
	plat = NULL;
#ifdef CONFIG_DM365
	plat = new PlatformCommonDM365;
#endif
#ifdef CONFIG_DM6446
	plat = new PlatformCommonDM6446;
#endif
#ifdef CONFIG_DM8168
	plat = new PlatformCommonDM8168;
#endif
}

int LmmCommon::init()
{
	QThreadPool::globalInstance()->setMaxThreadCount(5);
	initDebug();
	platformInit();
#ifdef USE_GSTREAMER
	gst_init(NULL, NULL);
#endif
	return 0;
}

int LmmCommon::installSignalHandlers(bool exitOnSigInt)
{
	quitOnSigInt = exitOnSigInt;
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

int LmmCommon::registerForSignal(int signal, BaseLmmElement *el)
{
	registeredElements[signal] << el;
	return 0;
}

int LmmCommon::registerForPipeSignal(BaseLmmElement *el)
{
	registeredElementsForPipe << el;
	return 0;
}

QString LmmCommon::getLibraryVersion()
{
	return VERSION_INFO;
}

#ifdef USE_LIVEMEDIA
	#include <liveMedia_version.hh>
#else
#define LIVEMEDIA_LIBRARY_VERSION_STRING "not used"
#endif
QString LmmCommon::getLiveMediaVersion()
{
	return LIVEMEDIA_LIBRARY_VERSION_STRING;
}


#ifdef CONFIG_VLC
	#include <vlc/libvlc.h>
#else
	#define libvlc_get_version() "not used"
#endif
QString LmmCommon::getLibVlcVersion()
{
	return libvlc_get_version();
}

QString LmmCommon::getFFmpegVersion()
{
#ifdef CONFIG_FFMPEG
	QString str;
	//str.append(avformat_configuration()).append("\n");
	str.append(QString("libavcodec: %1 %2 %3\n").arg(LIBAVCODEC_VERSION_MAJOR)
			   .arg(LIBAVCODEC_VERSION_MINOR).arg(LIBAVCODEC_VERSION_MICRO));
	str.append(QString("libavformat: %1 %2 %3\n").arg(LIBAVFORMAT_VERSION_MAJOR)
			   .arg(LIBAVFORMAT_VERSION_MINOR).arg(LIBAVFORMAT_VERSION_MICRO));
	str.append(QString("libavfilter: %1 %2 %3\n").arg(LIBAVFILTER_VERSION_MAJOR)
			   .arg(LIBAVFILTER_VERSION_MINOR).arg(LIBAVFILTER_VERSION_MICRO));
	str.append(QString("libavutil: %1 %2 %3\n").arg(LIBAVUTIL_VERSION_MAJOR)
			   .arg(LIBAVUTIL_VERSION_MINOR).arg(LIBAVUTIL_VERSION_MICRO));
	return str;
#else
	return "not used";
#endif
}

QString LmmCommon::getGStreamerVersion()
{
#if USE_GSTREAMER
#else
	return "not used";
#endif
}
