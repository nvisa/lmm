#ifndef H264ENCODER_H
#define H264ENCODER_H

#include "baselmmelement.h"
#include "textoverlay.h"
#include "lmmcommon.h"

#include <QTime>

class DmaiEncoder;
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

class H264Encoder : public BaseLmmElement
{
	Q_OBJECT
	/*Q_PROPERTY(bool useOverlay READ getUseOverlay WRITE setUseOverlay)
	Q_PROPERTY(bool useDisplay READ getUseDisplay WRITE setUseDisplay)
	Q_PROPERTY(bool useFile READ getUseFile WRITE setUseFile)
	Q_PROPERTY(bool useStreamOutput READ getUseStreamOutput WRITE setUseSteamOutput)
	Q_PROPERTY(bool threadedEncode READ getThreadedEncode WRITE setThreadedEncode)
	Q_PROPERTY(bool useFileIOForRtsp READ getRtspMethod WRITE setRtspMethod)
	Q_PROPERTY(Lmm::VideoOutput videoOutputType READ getVideoOutput WRITE setVideoOutput)
	Q_PROPERTY(StreamingProtocol streamingType READ getStreamingType WRITE setStreamingType)*/
public:
	enum StreamingProtocol {
		RTSP,
		PROPRIETY
	};

	explicit H264Encoder(QObject *parent = 0);
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
	DmaiEncoder *encoder;
	FileOutput *output;
	DM365VideoOutput *dm365Output;
	BaseLmmOutput *output3;
	V4l2Input *input;
	TextOverlay *overlay;
	QTimer *timer;
	BaseLmmOutput *rtspOutput;
	QTime timing;
	DebugServer *debugServer;
	EncodeThread *encodeThread;

	bool useFileIOForRtsp;
	bool useOverlay;
	bool useDisplay;
	bool useFile;
	bool useStreamOutput;
	bool threadedEncode;
	Lmm::VideoOutput videoOutputType;
	StreamingProtocol streamingType;

	/*void writeOverlaySettings(QXmlStreamWriter *wr);
	void writeCameraSettings(QXmlStreamWriter *wr);
	void writeStreamSettings(QXmlStreamWriter *wr);
	void writeEncodingSettings(QXmlStreamWriter *wr);
	void writeDisplaySettings(QXmlStreamWriter *wr);
	void writeNetworkSettings(QXmlStreamWriter *wr);
	void writeTimeSettings(QXmlStreamWriter *wr);
	void readOverlaySettings(QXmlStreamReader *xml, QString sectionName);
	void readCameraSettings(QXmlStreamReader *xml, QString sectionName);
	void readStreamSettings(QXmlStreamReader *xml, QString sectionName);
	void readDisplaySettings(QXmlStreamReader *xml, QString sectionName);
	void readNetworkSettings(QXmlStreamReader *xml, QString sectionName);
	void readTimeSettings(QXmlStreamReader *xml, QString sectionName);*/
};

#endif // H264ENCODER_H
