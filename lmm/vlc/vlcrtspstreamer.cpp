#include "vlcrtspstreamer.h"

#include <emdesk/debug.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_vlm.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>

static int imem_get(void *data, const char *cookie, int64_t *dts, int64_t *pts, unsigned *flags, size_t * bufferSize, void ** buffer)
{
	(void)cookie;
	VlcRtspStreamer *inst = (VlcRtspStreamer *)data;
	return inst->vlc_get(dts, pts, flags, bufferSize, buffer);
}

static int imem_release(void *data, const char *cookie, size_t bufferSize, void * buffer)
{
	(void)cookie;
	VlcRtspStreamer *inst = (VlcRtspStreamer *)data;
	return inst->vlc_release(bufferSize, buffer);
}

VlcRtspStreamer::VlcRtspStreamer(QObject *parent) :
	BaseLmmElement(parent)
{
	threaded = true;
	fps = 30;
}

int VlcRtspStreamer::start()
{
	frameCount = 0;
	char *vlc_args[11];
	vlc_args[0] = new char[50];
	vlc_args[1] = new char[50];
	vlc_args[2] = new char[50];
	vlc_args[5] = new char[50];
	sprintf(vlc_args[0], "--imem-get=%p", &imem_get);
	sprintf(vlc_args[1], "--imem-release=%p", &imem_release);
	sprintf(vlc_args[2], "--imem-data=%p", this);
	vlc_args[3] = (char *)"--imem-codec=H264";
	vlc_args[4] = (char *)"--imem-cat=2";
	sprintf(vlc_args[5], "--imem-fps=%d/1", fps);
	vlc_args[6] = (char *)"--imem-width=1280";
	vlc_args[7] = (char *)"--imem-height=720";
	vlc_args[8] = (char *)"--imem-caching=300";
	vlc_args[9] = (char *)"--imem-dar=1/1";
	vlc_args[10] = (char *)"-vvvv";
	libvlc_instance_t *vlc;
	vlc = libvlc_new(11, vlc_args);
#if 1
	//const char *sout = "#rtp{dst=192.168.1.1,port=1234,sdp=rtsp://192.168.1.199:8080/cam264.sdp}";
	const char *sout = "#rtp{sdp=rtsp://192.168.1.196:554/cam264.sdp}";
	const char *media_name = "cam264";
	libvlc_vlm_add_broadcast(vlc, media_name, "imem://", sout, 0, NULL, true, false);
	libvlc_vlm_play_media(vlc, media_name);
#else
	libvlc_media_t *media = libvlc_media_new_location(vlc, "imem://");
	//libvlc_media_add_option(media, ":sout=#rtp{dst=192.168.1.1,port=1234,sdp=rtsp://192.168.1.199:8080/cam264.sdp}");
	//libvlc_media_add_option(media, ":sout=#rtp{dst=192.168.1.1,port=1234");
	libvlc_media_add_option(media, ":sout=#rtp{sdp=rtsp://192.168.1.196:8080/cam264.sdp}");
	libvlc_media_player_t *mp = libvlc_media_player_new_from_media(media);
	libvlc_media_player_play(mp);
#endif

	return BaseLmmElement::start();
}

int VlcRtspStreamer::stop()
{
	/* TODO: free VLC resources */
	return BaseLmmElement::stop();
}

#include <QTime>
#include <QDateTime>

int VlcRtspStreamer::vlc_get(int64_t *dts, int64_t *pts, unsigned *flags, size_t *bufferSize, void **buffer)
{
	mInfo("start");
	(void)flags;
	if (inputBuffers.size()) {
		QTime t; t.start();
		inputLock.lock();
		RawBuffer buf = inputBuffers.first();
		*buffer = (void *)buf.constData();
		inputLock.unlock();
		*bufferSize = buf.size();
		mInfo("passing buffer to VLC with size %d %d", *bufferSize, t.elapsed());
		*dts = -1;
		static int base = 0;
		if (base == 0)
			base = QDateTime::currentDateTime().toTime_t();
		*pts = base + 1000000 / fps * frameCount++;
		updateOutputTimeStats();
	} else {
		*bufferSize = 0;
		mDebug("waiting");
	}
	mInfo("finish");
	return 0;
}

int VlcRtspStreamer::vlc_release(size_t bufferSize, void *buffer)
{
	mInfo("start: %p %d", buffer, bufferSize);
	if (bufferSize) {
		inputLock.lock();
		for (int i = 0; i < inputBuffers.size(); i++) {
			if (buffer == inputBuffers[i].constData()) {
				inputBuffers.removeAt(i);
				break;
			}
		}
		inputLock.unlock();
	}
	mInfo("finish");
	return 0;
}
