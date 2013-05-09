#ifndef MADDECODER_H
#define MADDECODER__H

#include <lmm/baselmmdecoder.h>
#include <QList>

struct mad_stream;
struct mad_frame;
struct mad_synth;
class RawBuffer;

class MadDecoder : public BaseLmmDecoder
{
	Q_OBJECT
public:
	explicit MadDecoder(QObject *parent = 0);
	int decode();
	int decodeBlocking();
	int decodeAll();
	int flush();
signals:
	
public slots:
private:
	struct mad_stream *stream;
	struct mad_frame *frame;
	struct mad_synth *synth;

	QByteArray madBuffer;

	int startDecoding();
	int stopDecoding();
};

#endif // MADMADDECODER__H
