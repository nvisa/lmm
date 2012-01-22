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

signals:

public slots:

private:
	struct ts_program *programs[8192];

};

#endif // TSDEMUX_H
