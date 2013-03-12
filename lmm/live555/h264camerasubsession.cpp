#include "h264camerasubsession.h"
#include "cameradevicesource.h"

#include "debug.h"

#include <H264VideoStreamFramer.hh>
#include <H264VideoRTPSink.hh>

H264CameraSubsession *H264CameraSubsession::
createNew(UsageEnvironment &env, const char *fileName, Boolean reuseFirstSource)
{
	return new H264CameraSubsession(env, fileName, reuseFirstSource);
}

H264CameraSubsession::~H264CameraSubsession()
{
}

H264CameraSubsession::H264CameraSubsession(UsageEnvironment &env,
										   const char *,
										   Boolean reuseFirstSource)
	: OnDemandServerMediaSubsession(env, reuseFirstSource)
{
	lastSrc = NULL;
}

FramedSource *H264CameraSubsession::
createNewStreamSource(unsigned, unsigned &estBitrate)
{
	qDebug("%s", __PRETTY_FUNCTION__);
	estBitrate = 4000; // kbps, estimate

	// Create the video source:
	DeviceParameters params;
	CameraDeviceSource *src = CameraDeviceSource::createNew(envir(), params);
	src->scheduler = scheduler;
	src->setSourceElement(dataSource);
	lastSrc = src;

	// Create a framer for the Video Elementary Stream:
	return H264VideoStreamFramer::createNew(envir(), src);
}

RTPSink *H264CameraSubsession::createNewRTPSink(
		Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
		FramedSource *)
{
	qDebug("%s", __PRETTY_FUNCTION__);
	OutPacketBuffer::maxSize = 500000;
	return H264VideoRTPSink::createNew(envir(),
									   rtpGroupsock, rtpPayloadTypeIfDynamic);
}
