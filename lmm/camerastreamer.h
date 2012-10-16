#ifndef CAMERASTREAMER_H
#define CAMERASTREAMER_H

#include "baselmmelement.h"
#include "textoverlay.h"
#include "lmmcommon.h"
#include "dmaiencoder.h"
#include "h264encoder.h"
#include "rtspoutput.h"

#include <QTime>

class FileOutput;
class V4l2Input;
class BaseLmmOutput;
class QTimer;
class TextOverlay;
class RtspOutput;
class DebugServer;
class EncodeThread;
class DM365VideoOutput;
class QXmlStreamWriter;
class QXmlStreamReader;
class BaseLmmMux;
class VlcRtspStreamer;
class RtspServer;
class JpegEncoder;

class CameraStreamer : public BaseLmmElement
{
	Q_OBJECT
public:
	enum StreamingProtocol {
		RTSP,
		PROPRIETY
	};

	explicit CameraStreamer(QObject *parent = 0);
	virtual int start();
	virtual int stop();

	/* control API */
	void addTextOverlay(QString text);
	void addTextOverlayParameter(TextOverlay::overlayTextFields field
								 , QString val = "");
	void setTextOverlayPosition(QPoint pos);
	void useDisplayOutput(bool v, Lmm::VideoOutput type = Lmm::COMPOSITE);
	void useFileOutput(QString fileName);
	void useStreamingOutput(bool v, StreamingProtocol proto = RTSP);
	void useThreadedEncode(bool v);
	const QByteArray getSettings();
	int setSettings(const QByteArray &data);
	DebugServer * getDebugServer() { return debugServer; }
	bool getFileIOForRtsp() { return useFileIOForRtsp; }
	void setFileIOForRtsp(bool v) { useFileIOForRtsp = v; }
	void setUseOverlay (bool v) { useOverlay = v; }
	bool getUseOverlay () { return useOverlay; }
	void setUseDisplay (bool v) { useDisplay = v; }
	bool getUseDisplay () { return useDisplay; }
	void setUseFile (bool v) { useFile = v; }
	bool getUseFile () { return useFile; }
	void setUseStreamOutput (bool v) { useStreamOutput = v; }
	bool getUseStreamOutput () { return useStreamOutput; }
	bool getUseTestInput() { return useTestInput; }
	void setUseTestInput(bool v) { useTestInput = v; }
	TextOverlay * getTextOverlay() { return overlay; }
	BaseLmmElement *getTestInput() { return testInput; }
	void setStreamingType (StreamingProtocol v) { streamingType = v; }
	StreamingProtocol getStreamingType () { return streamingType; }
	Lmm::VideoOutput getVideoOutputType() { return videoOutputType; }
	H264Encoder * getH264Encoder() { return h264Encoder; }
	bool getThreadedEncode() { return threadedEncode; }

	/* settings */
	void writeImplementationSettings(QXmlStreamWriter *wr);
	void readImplementationSettings(QXmlStreamReader *xml, QString sectionName);
signals:
public slots:
	void encodeLoop();
	void flushElements();
	void newRtspSessionCreated(RtspOutput::sessionType type);
private:
	QList<BaseLmmElement *> elements;
	DmaiEncoder *encoder;
	H264Encoder *h264Encoder;
	JpegEncoder *jpegEncoder;
	FileOutput *output;
	DM365VideoOutput *dm365Output;
	BaseLmmOutput *output3;
	FileOutput *jpegFileOutput;
	V4l2Input *input;
	BaseLmmElement *testInput;
	TextOverlay *overlay;
	QTimer *timer;
	RtspOutput *rtspOutput;
	RtspServer *rtspServer;
	QTime timing;
	DebugServer *debugServer;
	EncodeThread *encodeThread;
	EncodeThread *jpegEncodeThread;
	BaseLmmMux *mux;
	VlcRtspStreamer *rtspVlc;
	int muxType;
	bool flushForSpsPps;
	QTime jpegTime;
	int jpegShotInterval;

	int imageWidth;
	int imageHeight;

	bool useFileIOForRtsp;
	bool useOverlay;
	bool useDisplay;
	bool useFile;
	bool useStreamOutput;
	bool threadedEncode;
	bool useTestInput;
	Lmm::VideoOutput videoOutputType;
	StreamingProtocol streamingType;
	RtspOutput::sessionType sessionType;

	Lmm::CodecType getSessionCodec(RtspOutput::sessionType type);
};

#endif // CAMERASTREAMER_H
