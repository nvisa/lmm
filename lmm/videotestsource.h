#ifndef VIDEOTESTSOURCE_H
#define VIDEOTESTSOURCE_H

#include "baselmmelement.h"

#include <QImage>

class VideoTestSource : public BaseLmmElement
{
	Q_OBJECT
public:
	enum TestPattern {
		COLORBARS,
		COLORCHART,
		COLORSQUARES,
		GRADIENT_16,
		HEX_9X5,
		LINEAR_ZONEPLATE,
		SQUARE_WEDGES,
		STAR_BARS_144,
		STAR_BARS_FULL,
		STAR_SINE_144,
		TRT,
		ZONE_HARDEDGE,

		PATTERN_COUNT
	};

	explicit VideoTestSource(QObject *parent = 0);
	void setTestPattern(TestPattern p);
	void setFps(int fps);

	RawBuffer nextBuffer();
signals:

public slots:
private:
	int targetFps;
	int width;
	int height;
	qint64 lastBufferTime;
	int bufferTime;
	TestPattern pattern;
	QImage pImage;
	RawBuffer imageBuf;

	QImage getPatternImage(TestPattern p);
};

#endif // VIDEOTESTSOURCE_H
