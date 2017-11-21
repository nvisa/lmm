#include "desktopstreamer.h"

#include <QMap>
#include <QApplication>

#include <lmm/debug.h>
#include <lmm/lmmcommon.h>

static const QMap<QString, QString> parseArgs(int &argc, char **argv)
{
	QMap<QString, QString> args;
	args.insert("app", QString::fromLatin1(argv[0]));
	QStringList files;
	QString cmd;
	for (int i = 1; i < argc; i++) {
		QString arg = QString::fromLatin1(argv[i]);
		cmd.append(arg).append(" ");
		if (!arg.startsWith("--") && !arg.startsWith("-")) {
			files << arg;
			continue;
		}
		QString pars;
		if (i + 1 < argc)
			pars = QString::fromLatin1(argv[i + 1]);
		if (!pars.startsWith("--") && !pars.startsWith("-")) {
			i++;
			args.insert(arg, pars);
			cmd.append(pars).append(" ");
		} else
			args.insert(arg, "");
	}
	args.insert("__app__", QString::fromLatin1(argv[0]));
	args.insert("__cmd__", cmd);
	args.insert("__files__", files.join("\n"));
	return args;
}

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	LmmCommon::init();

	QMap<QString, QString> args = parseArgs(argc, argv);
	DesktopStreamer s;
	s.setCapture();
	s.start();

	return a.exec();
}

