#include "rtspserver.h"
#include "live555/h264camerasubsession.h"
#include "live555/cameradevicesource.h"

#include <emdesk/debug.h>

#include <BasicUsageEnvironment.hh>
#include <RTSPServer.hh>
#include <H264VideoFileServerMediaSubsession.hh>
#include <MPEG4VideoFileServerMediaSubsession.hh>

#include <errno.h>

#include <QThread>

class LiveRtspServer : public RTSPServer
{
protected:
	LiveRtspServer(UsageEnvironment& env, int ourSocket, Port ourPort,
			    UserAuthenticationDatabase* authDatabase,
					  unsigned reclamationTestSeconds)
		: RTSPServer(env, ourSocket, ourPort, authDatabase, reclamationTestSeconds)
	{
	}
	ServerMediaSession* lookupServerMediaSession(char const* streamName)
	{
		qDebug("%s: %s", __PRETTY_FUNCTION__, streamName);
		return NULL;
	}
};

class Live555Thread : public QThread
{
public:
	H264CameraSubsession *subs;
	TaskScheduler* scheduler;
	Live555Thread(RtspServer *parent)
	{
		parentServer = parent;
	}
	int init()
	{
		// Begin by setting up our usage environment:
		scheduler = BasicTaskScheduler::createNew();
		env = BasicUsageEnvironment::createNew(*scheduler);

		UserAuthenticationDatabase* authDB = NULL; /* TODO: User access control */
		// Create the RTSP server.
		// and then with the alternative port number (8554):
		RTSPServer* rtspServer;
		portNumBits rtspServerPortNum = 8554;
		rtspServer = RTSPServer::createNew(*env, rtspServerPortNum, authDB);
		if (rtspServer == NULL) {
			mDebug("Failed to create RTSP server: %s", env->getResultMsg());
			return -EINVAL;
		}

		*env << "LIVE555 Media Server\n";
		*env << "\tversion " << "0.0.1"
			 << " (LIVE555 Streaming Media library version "
			 << LIVEMEDIA_LIBRARY_VERSION_STRING << ").\n";

		char* urlPrefix = rtspServer->rtspURLPrefix();
		*env << "Play streams from this server using the URL\n\t"
			 << urlPrefix << "<filename>\nwhere <filename> is a file present in the current directory.\n";
		*env << "Each file's type is inferred from its name suffix:\n";
		*env << "\t\".264\" => a H.264 Video Elementary Stream file\n";
		*env << "\t\".aac\" => an AAC Audio (ADTS format) file\n";
		*env << "\t\".ac3\" => an AC-3 Audio file\n";
		*env << "\t\".amr\" => an AMR Audio file\n";
		*env << "\t\".dv\" => a DV Video file\n";
		*env << "\t\".m4e\" => a MPEG-4 Video Elementary Stream file\n";
		*env << "\t\".mkv\" => a Matroska audio+video+(optional)subtitles file\n";
		*env << "\t\".mp3\" => a MPEG-1 or 2 Audio file\n";
		*env << "\t\".mpg\" => a MPEG-1 or 2 Program Stream (audio+video) file\n";
		*env << "\t\".ts\" => a MPEG Transport Stream file\n";
		*env << "\t\t(a \".tsx\" index file - if present - provides server 'trick play' support)\n";
		*env << "\t\".wav\" => a WAV Audio file\n";
		*env << "\t\".webm\" => a WebM audio(Vorbis)+video(VP8) file\n";
		*env << "See http://www.live555.com/mediaServer/ for additional documentation.\n";

		ServerMediaSession *sms = ServerMediaSession::
				createNew(*env, "camera", "camera" ,
						  "720p 30 fps camera stream");
		subs = H264CameraSubsession::
				createNew(*env, "camera", true);
		subs->scheduler = scheduler;
		subs->setRstpElement(parentServer);
		sms->addSubsession(subs);
		rtspServer->addServerMediaSession(sms);
		/*char* url = rtspServer->rtspURL(sms);
		//UsageEnvironment& env = rtspServer->envir();
		*env << "\n\"" << "cam" << "\" stream, from the file \""
			<< "cam.264" << "\"\n";
		*env << "Play this stream using the URL \"" << url << "\"\n";
		delete[] url;*/

		return 0;
	}

	void stop()
	{
		exit = true;
	}

	void run()
	{
		exit = false;
		if (init() == 0)
			env->taskScheduler().doEventLoop();
	}
private:
	bool exit;
	RtspServer *parentServer;

	UsageEnvironment* env;
};

RtspServer::RtspServer(QObject *parent) :
	BaseLmmElement(parent)
{
	liveThread = NULL;
}

int RtspServer::start()
{
	liveThread = new Live555Thread(this);
	liveThread->start(QThread::LowestPriority);
	return 0;
}

int RtspServer::stop()
{
	liveThread->terminate();
	liveThread->wait();
	liveThread->deleteLater();
	liveThread = NULL;
	return 0;
}

int RtspServer::addBuffer(RawBuffer buffer)
{
	if (buffer.size() == 0)
		return -EINVAL;
	/*RawBuffer copy(buffer.data(), buffer.size());
	copy.setStreamBufferNo(buffer.streamBufferNo());
	receivedBufferCount++;*/
	outputBuffers << buffer;
	{
		extern void signalNewCameraFrameData();
		signalNewCameraFrameData();
	}
	return 0;
	if (buffer.size() == 0)
		return -EINVAL;
	CameraDeviceSource *src = liveThread->subs->lastDeviceSource();
	if (src) {
		receivedBufferCount++;
		RawBuffer buf(buffer.data(), buffer.size());
		src->addBuffer(buf);
	}
	return 0;
}
