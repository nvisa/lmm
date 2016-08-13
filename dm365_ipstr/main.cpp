#include <QStringList>
#include <QCoreApplication>

#include <lmm/dm365/cvbsstreamer.h>
#include <lmm/dm365/ipcamerastreamer.h>
#include <lmm/dm365/simplertpstreamer.h>
#include <lmm/dm365/simple1080pstreamer.h>

#include <ecl/settings/applicationsettings.h>

#include "genericstreamer.h"

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	LmmCommon::init();
	ApplicationSettings::instance()->load("/etc/dm365_ipstr.json");

	BaseStreamer *s = NULL;
	if (a.arguments().contains("--1080p"))
		s = new Simple1080pStreamer;
	else if (a.arguments().contains("--cvbs"))
		s = new CVBSStreamer;
	else
		s = new GenericStreamer;
	s->start();

	return a.exec();
}
