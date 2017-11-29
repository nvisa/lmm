#ifndef TK1VIDEOSTREAMER_H
#define TK1VIDEOSTREAMER_H

#include <QObject>

#include <lmm/players/basestreamer.h>

class RtspClient;
class RtpReceiver;
class UvcVideoInput;
class RtpTransmitter;
class LmmGstPipeline;
class FFmpegColorSpace;
class MetaDataManager;

class TK1VideoStreamer : public BaseStreamer
{
	Q_OBJECT
public:
	explicit TK1VideoStreamer(QObject *parent = 0);
	int serveRtsp(const QString &ip, const QString &stream);
	int viewSource(const QString &ip, const QString &stream);
	int serveRtp(const QString &ip, const QString &stream, const QString &dstIp, quint16 dstPort);

	int viewAnalogGst(const QString &device);
signals:

protected:
	virtual int pipelineOutput(BaseLmmPipeline *, const RawBuffer &);
	int processFrame(const RawBuffer &buf);

	UvcVideoInput *vin;
	RtpReceiver *rtp;
	RtspClient *rtsp;
	LmmGstPipeline *gst1;
	LmmGstPipeline *gst2;
	RtpTransmitter *rtpout;
	BaseRtspServer *rtspServer;
	MetaDataManager *metaMan;
	FFmpegColorSpace *cspc;
};

#endif // TK1VIDEOSTREAMER_H
