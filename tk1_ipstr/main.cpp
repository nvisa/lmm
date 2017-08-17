#include "tk1videostreamer.h"

#include <QMap>
#include <QCoreApplication>

#include <lmm/debug.h>

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
	QCoreApplication a(argc, argv);

	LmmCommon::init();

	QMap<QString, QString> args = parseArgs(argc, argv);

	TK1VideoStreamer s;
	if (args.contains("--view"))
		s.viewSource(args["--target"], args["--stream"]);
	else
		s.serveRtsp(args["--target"], args["--stream"]);
	s.start();

	return a.exec();
}

