#ifndef GENERICVIDEOTESTSOURCE_H
#define GENERICVIDEOTESTSOURCE_H

#include <lmm/baselmmelement.h>

#include <QFile>
#include <QImage>

class QFile;
class TimeoutThread;
class LmmBufferPool;

class GenericVideoTestSource : public BaseLmmElement
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
		RAW_YUV_FILE,
		RAW_YUV_VIDEO,

		PATTERN_COUNT
	};

	explicit GenericVideoTestSource(QObject *parent = 0);
	explicit GenericVideoTestSource(int nWidth, int nHeight, QObject *parent = 0);
	int setTestPattern(TestPattern p);
	TestPattern getPattern() { return pattern; }
	void setFps(int fps);
	void setYUVFile(QString filename);
	void setYUVVideo(QString filename, bool loop = false);

	int processBlocking(int ch = 0);
	void aboutToDeleteBuffer(const RawBufferParameters *params);
	int flush();

	int setSetting(const QString &setting, const QVariant &value);
	QVariant getSetting(const QString &setting);
signals:

private slots:
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
	TimeoutThread *tt;
	QFile videoFile;
	bool loopVideoFile;
	LmmBufferPool *pool;
	QHash<int, RawBuffer> refBuffers;

	QImage getPatternImage(TestPattern p);
	RawBuffer addNoise(RawBuffer imageBuf);
	bool checkCache(TestPattern p);
	int processBuffer(const RawBuffer &buf);
	void addBufferToPool(RawBuffer buf);
};

#endif // GENERICVIDEOTESTSOURCE_H
