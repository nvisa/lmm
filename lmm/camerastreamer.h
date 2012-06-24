#ifndef CAMERASTREAMER_H
#define CAMERASTREAMER_H

#include "baselmmelement.h"
#include "textoverlay.h"
#include "lmmcommon.h"
#include "dmaiencoder.h"
#include "h264encoder.h"

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
	TextOverlay * getTextOverlay() { return overlay; }
	void setStreamingType (StreamingProtocol v) { streamingType = v; }
	StreamingProtocol getStreamingType () { return streamingType; }
	Lmm::VideoOutput getVideoOutputType() { return videoOutputType; }
	DmaiEncoder * getEncoder() { return encoder; }
	bool getThreadedEncode() { return threadedEncode; }

	/* settings */
	void writeImplementationSettings(QXmlStreamWriter *wr);
	void readImplementationSettings(QXmlStreamReader *xml, QString sectionName);
signals:
public slots:
	void encodeLoop();
	void flushElements();
private:
	QList<BaseLmmElement *> elements;
	H264Encoder *encoder;
	FileOutput *output;
	DM365VideoOutput *dm365Output;
	BaseLmmOutput *output3;
	V4l2Input *input;
	BaseLmmElement *testInput;
	TextOverlay *overlay;
	QTimer *timer;
	BaseLmmOutput *rtspOutput;
	QTime timing;
	DebugServer *debugServer;
	EncodeThread *encodeThread;
	BaseLmmMux *mux;
	int muxType;

	bool useFileIOForRtsp;
	bool useOverlay;
	bool useDisplay;
	bool useFile;
	bool useStreamOutput;
	bool threadedEncode;
	bool useTestInput;
	Lmm::VideoOutput videoOutputType;
	StreamingProtocol streamingType;
};

#endif // CAMERASTREAMER_H
