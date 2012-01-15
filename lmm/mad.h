#ifndef MAD_H
#define MAD_H

#include <QObject>
#include <QList>

struct mad_stream;
struct mad_frame;
struct mad_synth;
class RawBuffer;

class Mad : public QObject
{
	Q_OBJECT
public:
	explicit Mad(QObject *parent = 0);
	int addBuffer(RawBuffer *buf);
	int decode();
	RawBuffer * nextBuffer();
signals:
	
public slots:
private:
	struct mad_stream *stream;
	struct mad_frame *frame;
	struct mad_synth *synth;
	QList<RawBuffer *> buffers;
	QList<RawBuffer *> outputBuffers;
};

#endif // MAD_H
