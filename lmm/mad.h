#ifndef MAD_H
#define MAD_H

#include "baselmmdecoder.h"
#include <QList>

struct mad_stream;
struct mad_frame;
struct mad_synth;
class RawBuffer;

class Mad : public BaseLmmDecoder
{
	Q_OBJECT
public:
	explicit Mad(QObject *parent = 0);
	int decode();
	int decodeAll();
	int start();
	int stop();
signals:
	
public slots:
private:
	struct mad_stream *stream;
	struct mad_frame *frame;
	struct mad_synth *synth;

	QByteArray madBuffer;
};

#endif // MAD_H
