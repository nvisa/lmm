#ifndef VIDEOTESTSOURCE_H
#define VIDEOTESTSOURCE_H

#include "baselmmelement.h"

#include <QImage>

class QFile;
class DmaiBuffer;

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
	explicit VideoTestSource(int nWidth, int nHeight, QObject *parent = 0);
	void setTestPattern(TestPattern p);
	TestPattern getPattern() { return pattern; }
	void setFps(int fps);

	RawBuffer nextBuffer();
	int flush();
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
	bool noisy;
	QList<char *> noise;
	int noiseWidth;
	int noiseHeight;

	QImage getPatternImage(TestPattern p);
	DmaiBuffer addNoise(DmaiBuffer imageBuf);
};

#endif // VIDEOTESTSOURCE_H
