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
	enum overlayType {
		SIMPLE,
		CHAR_MAP,
		PIXEL_MAP
	};
	explicit TextOverlay(overlayType t = CHAR_MAP, QObject *parent = 0);
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
	QList<QList<int> > charPixelMap;
	QList<int> charFontWidth;
	int fontSize;
	int fontHeight;
	overlayType type;

	QByteArray createCharMap(int fontWidth, const QImage &image);
	QList<int> createPixelMap(int fontWidth, const QImage &image);
	void createYuvMap();
	int dmaCopy(void *src, void *dst, QImage *im);
	void createAsciiTable();
	void yuvSwOverlay(RawBuffer *buffer);
	void yuvSwMapOverlay(RawBuffer *buffer);
	void yuvSwPixmapOverlay(RawBuffer *buffer);
};

#endif // TEXTOVERLAY_H
