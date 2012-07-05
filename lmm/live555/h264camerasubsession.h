#ifndef H264CAMERASUBSESSION_H
#define H264CAMERASUBSESSION_H

#include <OnDemandServerMediaSubsession.hh>
#include <UsageEnvironment.hh>

class FramedSource;
class Groupsock;
class CameraDeviceSource;
class BaseLmmElement;

class H264CameraSubsession : public OnDemandServerMediaSubsession
{
public:
	static H264CameraSubsession*
	  createNew(UsageEnvironment& env, char const* fileName,
				Boolean reuseFirstSource);
	~H264CameraSubsession();
	CameraDeviceSource * lastDeviceSource() { return lastSrc; }
	void setRstpElement(BaseLmmElement *el) { dataSource = el; }

	TaskScheduler *scheduler;
protected:
	H264CameraSubsession(UsageEnvironment& env,
						 char const* fileName, Boolean reuseFirstSource);
	FramedSource* createNewStreamSource(unsigned clientSessionId,
												unsigned& estBitrate);
	// "estBitrate" is the stream's estimated bitrate, in kbps
	RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
									  unsigned char rtpPayloadTypeIfDynamic,
									  FramedSource* inputSource);
private:
	CameraDeviceSource *lastSrc;
	BaseLmmElement *dataSource;
};

#endif // H264CAMERASUBSESSION_H
