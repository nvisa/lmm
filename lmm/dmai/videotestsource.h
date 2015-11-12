#ifndef VIDEOTESTSOURCE_H
#define VIDEOTESTSOURCE_H

#include <lmm/baselmmelement.h>

#include <QFile>
#include <QImage>

class QFile;
class DmaiBuffer;
class TimeoutThread;
class LmmBufferPool;
struct BufferGfx_Attrs;

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
		RAW_YUV_FILE,
		RAW_YUV_VIDEO,

		PATTERN_COUNT
	};

	explicit VideoTestSource(QObject *parent = 0);
	explicit VideoTestSource(int nWidth, int nHeight, QObject *parent = 0);
	int setTestPattern(TestPattern p);
	TestPattern getPattern() { return pattern; }
	void setFps(int fps);
	void setYUVFile(QString filename);
	void setYUVVideo(QString filename, bool loop = false);

	void aboutToDeleteBuffer(const RawBufferParameters *params);
	int flush();
	int start();
	int stop();

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
	QMap<int, RawBuffer> refBuffers;
	QFile videoFile;
	bool loopVideoFile;
	LmmBufferPool *pool;

	friend class TimeoutThread;
	QImage getPatternImage(TestPattern p);
	DmaiBuffer addNoise(DmaiBuffer imageBuf);
	bool checkCache(TestPattern p, BufferGfx_Attrs *attr);
	int processBuffer(const RawBuffer &buf);
	void addBufferToPool(RawBuffer buf);
};

#endif // VIDEOTESTSOURCE_H
