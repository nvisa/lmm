#ifndef TEXTOVERLAY_H
#define TEXTOVERLAY_H

#include <lmm/baselmmelement.h>

#include <QPaintDevice>
#include <QPaintEngine>
#include <QMutex>
#include <QList>
#include <QMap>

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
		FIELD_FRAME_TIME,
		FIELD_SETTING,
	};
	explicit TextOverlay(overlayType t = CHAR_MAP, QObject *parent = 0);
	int setFontSize(int size);
	int setOverlayPosition(QPoint topLeft) { overlayPos = topLeft; return 0; }
	int setOverlayText(QString text);
	void clearFields();
	void addOverlayField(overlayTextFields f, QString val = "");
	int getFontSize() { return fontSize; }
	QStringList getFontSizes();
	QPoint getOverlayPosition() { return overlayPos; }
	QString getOverlayText() { return overlayText; }
	int getFieldCount() { return overlayFields.size(); }
	overlayTextFields getOverlayField(int pos) { return overlayFields[pos]; }
	QString getOverlayFieldText(int pos) { return overlayFieldTexts[pos]; }
	int setOverlayField(int pos, overlayTextFields f);
	int setOverlayFieldText(int pos, QString text);
	int start();
	int stop();
	int overlayInPlace(const RawBuffer &buffer);

	int setSetting(const QString &setting, const QVariant &value);
	QVariant getSetting(const QString &setting);
protected:
	int processBuffer(const RawBuffer &buffer);
signals:

public slots:
private:
	int mmapfd;
	QMutex dmalock;
	QMutex maplock;
	QList<QByteArray> charMap;
	QList<QList<int> > charPixelMap;
	QList<int> charFontWidth;
	int fontSize;
	int fontHeight;
	overlayType type;
	QPoint overlayPos;
	QString overlayText;
	QList<overlayTextFields> overlayFields;
	QStringList overlayFieldTexts;
	int mapCount;

	bool readMapsFromCache();
	QByteArray createCharMap(int fontWidth, const QImage &image);
	QList<int> createPixelMap(int fontWidth, const QImage &image);
	void createYuvMap();
	void yuvSwOverlay(RawBuffer buffer);
	void yuvSwMapOverlay(RawBuffer buffer);
	void yuvSwPixmapOverlay(RawBuffer buffer);
	QString compileOverlayText(const RawBuffer &buf);
};

#endif // TEXTOVERLAY_H
