#ifndef TEXTOVERLAY_H
#define TEXTOVERLAY_H

#include "baselmmelement.h"

#include <QPaintDevice>
#include <QPaintEngine>
#include <QMutex>
#include <QList>
#include <QMap>

class RawBuffer;
struct _Buffer_Object;
typedef struct _Buffer_Object *Buffer_Handle;

class TextOverlay : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit TextOverlay(QObject *parent = 0);
	int start();
	int stop();
	int addBuffer(RawBuffer *buffer);
signals:
	
public slots:
private:
	int mmapfd;
	QMutex dmalock;
	Buffer_Handle imageBuf;
	QList<QByteArray> charMap;
	int fontSize;
	int fontHeight;

	void createCharMap();
	int dmaCopy(void *src, void *dst, QImage *im);
	void createAsciiTable();
};

#endif // TEXTOVERLAY_H
