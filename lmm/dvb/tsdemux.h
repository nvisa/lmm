#ifndef TSDEMUX_H
#define TSDEMUX_H

#include <QObject>

struct ts_program;

class tsDemux : public QObject
{
	Q_OBJECT
public:
	explicit tsDemux(QObject *parent = 0);
	int newData(unsigned char *data);
	const char * getStream(int pid, int *size);
	static qint64 parsePcr(const unsigned char *data);
	static int parsePmt(const unsigned char *data);
signals:

public slots:

private:
	struct ts_program *programs[8192];

};

#endif // TSDEMUX_H
