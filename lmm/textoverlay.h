#ifndef TEXTOVERLAY_H
#define TEXTOVERLAY_H

#include <lmm/baselmmelement.h>

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
	enum overlayTextFields {
		FIELD_CURRENT_DATE,
		FIELD_CURRENT_TIME,
		FIELD_CURRENT_DATETIME,
		FIELD_FRAME_NO,
		FIELD_STATIC_TEXT,
		FIELD_STREAM_TIME,
		FIELD_STREAM_FPS,
		FIELD_AVG_CPU_LOAD,
	};
	explicit TextOverlay(overlayType t = CHAR_MAP, QObject *parent = 0);
	int setFontSize(int size);
	int setOverlayPosition(QPoint topLeft) { overlayPos = topLeft; return 0; }
	int setOverlayText(QString text);
	void addOverlayField(overlayTextFields f, QString val = "");
	int getFontSize() { return fontSize; }
	QPoint getOverlayPosition() { return overlayPos; }
	QString getOverlayText() { return overlayText; }
	int getFieldCount() { return overlayFields.size(); }
	overlayTextFields getOverlayField(int pos) { return overlayFields[pos]; }
	QString getOverlayFieldText(int pos) { return overlayFieldTexts[pos]; }
	int setOverlayField(int pos, overlayTextFields f);
	int setOverlayFieldText(int pos, QString text);
	int start();
	int stop();
protected:
	int processBuffer(RawBuffer buffer);
signals:
	
public slots:
private:
	int mmapfd;
	QMutex dmalock;
	QMutex maplock;
	Buffer_Handle dmaBuf;
	QList<QByteArray> charMap;
	QList<QList<int> > charPixelMap;
	QList<int> charFontWidth;
	int fontSize;
	int fontHeight;
	overlayType type;
	bool useDma;
	void *imageBuf;
	QPoint overlayPos;
	QString overlayText;
	QList<overlayTextFields> overlayFields;
	QStringList overlayFieldTexts;

	bool readMapsFromCache();
	QByteArray createCharMap(int fontWidth, const QImage &image);
	QList<int> createPixelMap(int fontWidth, const QImage &image);
	void createYuvMap();
	int dmaCopy(void *src, void *dst, QImage *im);
	void yuvSwOverlay(RawBuffer buffer);
	void yuvSwMapOverlay(RawBuffer buffer);
	void yuvSwPixmapOverlay(RawBuffer buffer);
	QString compileOverlayText();
};

#endif // TEXTOVERLAY_H
