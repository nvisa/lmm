#include "alsa.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/global.h>
#include <alsa/control.h>
#include <alsa/asoundlib.h>
#include <alsa/control.h>
#include <alsa/error.h>
#include "emdesk/debug.h"
#include "emdesk/platform_info.h"

Alsa::Alsa(QObject *parent) :
	QObject(parent)
{
	bytesPerSample = 2;
	bufferTime = 200000;
	periodTime = 10000;
	handle = NULL;
	hctl = NULL;
}

int Alsa::open(int rate)
{
	if (!hctl)
		initVolumeControl();
	int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err)
		return err;
	mDebug("pcm device opened");
	err = setHwParams(rate);
	if (err)
		return err;
	mDebug("hw params ok");
	err = setSwParams();
	if (err)
		return err;
	mDebug("sw params ok");
	running = true;
	return 0;
}

int Alsa::close()
{
	mutex.lock();
	int err = snd_pcm_close(handle);
	handle = NULL;
	running = false;
	mutex.unlock();
	return err;
}

int Alsa::pause()
{
	mutex.lock();
	int err = snd_pcm_pause(handle, 1);
	mutex.unlock();
	running = false;
	return err;
}

int Alsa::resume()
{
	mutex.lock();
	int err = snd_pcm_pause(handle, 0);
	mutex.unlock();
	running = true;
	return err;
}

static int xrun_recovery(snd_pcm_t *handle, int err)
{
	fDebug("xrun recovery %d", err);

	if (err == -EPIPE) {          /* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			fDebug("Can't recovery from underrun, prepare failed: %s", snd_strerror (err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			usleep (100);           /* wait until the suspend flag is released */

		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				fDebug("Can't recovery from suspend, prepare failed: %s",
									snd_strerror (err));
		}
		return 0;
	}
	return err;
}

int Alsa::write(const void *buf, int length)
{
	int err;
	int cptr;
	short *ptr = (short *)buf;

	if (!handle || !running)
		return length;
	mutex.lock();
	cptr = length / bytesPerSample / 2;

	while (cptr > 0) {
		err = snd_pcm_writei(handle, ptr, cptr);
		mInfo("written %d frames out of %d", err, cptr);
		if (err < 0) {
			mDebug("Write error: %s", snd_strerror (err));
			if (err == -EAGAIN) {
				continue;
			} else if (xrun_recovery(handle, err) < 0) {
				goto write_error;
			}
			continue;
		}

		ptr += snd_pcm_frames_to_bytes(handle, err);
		cptr -= err;
	}

	mutex.unlock();
	return length - (cptr * bytesPerSample);

write_error:
	{
		mutex.unlock();
		return length; /* skip one period */
	}
}

int Alsa::delay()
{
	if (!handle)
		return 0;
	snd_pcm_sframes_t delay;
	if (snd_pcm_delay(handle, &delay))
		return 0;
	return 1000000ll * delay / sampleRate;
}

int Alsa::setHwParams(int rate)
{
	unsigned int rrate = rate;
	if (rrate == 0)
		rrate = 48000;
	int err;
	snd_pcm_hw_params_t *params;

	snd_pcm_hw_params_malloc(&params);

	err = snd_pcm_hw_params_any(handle, params);
	if (err)
		goto out;
	err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err)
		goto out;
	mDebug("access type ok");
	err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16);
	if (err)
		goto out;
	mDebug("format ok");
	snd_pcm_hw_params_set_channels(handle, params, 2);
	if (err)
		goto out;
	mDebug("channels ok");
	sampleRate = rrate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, NULL);
	if (err)
		goto out;
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &bufferTime, NULL);
	if (err)
		goto out;
	err = snd_pcm_hw_params_set_period_time_near(handle, params, &periodTime, NULL);
	if (err)
		goto out;
	snd_pcm_hw_params_get_buffer_size(params, &bufferSize);
	snd_pcm_hw_params_get_period_size(params, &periodSize, NULL);
	mDebug("rate=%d bufferSize=%lu bufferTime=%d periodSize=%lu periodTime=%d",
			rrate, bufferSize, bufferTime, periodSize, periodTime);
	//snd_pcm_hw_params_get_buffer_time(params, &bufferTime, NULL);
	//snd_pcm_hw_params_get_period_time(params, &periodTime, NULL);
	snd_pcm_hw_params(handle, params);
out:
	snd_pcm_hw_params_free(params);
	return err;
}

int Alsa::setSwParams()
{
	int err;
	snd_pcm_sw_params_t *params;
	snd_pcm_sw_params_malloc(&params);
	mDebug("start");
	err = snd_pcm_sw_params_current(handle, params);
	if (err)
		goto out;
	mDebug("current ok");
	err = snd_pcm_sw_params_set_start_threshold(handle, params, bufferSize / periodSize * periodSize);
	//err = snd_pcm_sw_params_set_start_threshold(handle, params, periodSize * 6);
	if (err)
		goto out;
	mDebug("threshold ok");
	err = snd_pcm_sw_params_set_avail_min(handle, params, periodSize);
	if (err)
		goto out;
	mDebug("minimum ok");
	err = snd_pcm_sw_params(handle, params);

out:
	snd_pcm_sw_params_free(params);
	return err;
}

void Alsa::initVolumeControl()
{
	QString volumeControlName;
	QString switchControlName;
	if (cpu_is_x86())
		volumeControlName = "Master Playback Volume";
	else
		volumeControlName = "PCM Playback Volume";
	if (cpu_is_x86())
		switchControlName = "Master Playback Switch";
	else
		switchControlName = "HP Playback Switch";
	/* init sound card interface */
	int err;
	err = snd_hctl_open(&hctl, "default", 0);
	if (err == 0 && hctl) {
		err = snd_hctl_load(hctl);
		if (err < 0) {
			snd_hctl_close(hctl);
			hctl = NULL;
		} else {
			snd_ctl_elem_id_malloc(&mixerVolumeElemId);
			snd_ctl_elem_id_set_interface(mixerVolumeElemId, SND_CTL_ELEM_IFACE_MIXER);
			snd_ctl_elem_id_set_name(mixerVolumeElemId, qPrintable(volumeControlName));
			mixerVolumeElem = snd_hctl_find_elem(hctl, mixerVolumeElemId);
			if (!mixerVolumeElem) {
				snd_hctl_close(hctl);
				hctl = NULL;
			} else {
				snd_ctl_elem_value_malloc(&mixerVolumeControl);
				snd_ctl_elem_value_set_id(mixerVolumeControl, mixerVolumeElemId);

				snd_ctl_elem_id_malloc(&mixerSwitchElemId);
				snd_ctl_elem_id_set_interface(mixerSwitchElemId, SND_CTL_ELEM_IFACE_MIXER);
				snd_ctl_elem_id_set_name(mixerSwitchElemId, qPrintable(switchControlName));
				mixerSwitchElem = snd_hctl_find_elem(hctl, mixerSwitchElemId);
				if (!mixerSwitchElem) {
					snd_hctl_close(hctl);
					hctl = NULL;
				} else {
					snd_ctl_elem_value_malloc(&mixerSwitchControl);
					snd_ctl_elem_value_set_id(mixerSwitchControl, mixerSwitchElemId);
				}
			}
		}
	} else
		hctl = NULL;
}

int Alsa::mute(bool mute)
{
	if (!hctl)
		initVolumeControl();
	if (mute) {
		snd_ctl_elem_value_set_integer(mixerSwitchControl, 0, 0);
		snd_ctl_elem_value_set_integer(mixerSwitchControl, 1, 0);
	} else {
		snd_ctl_elem_value_set_integer(mixerSwitchControl, 0, 1);
		snd_ctl_elem_value_set_integer(mixerSwitchControl, 1, 1);
	}
	return snd_hctl_elem_write(mixerSwitchElem, mixerSwitchControl);
}

int Alsa::currentVolumeLevel()
{
	if (!hctl)
		initVolumeControl();
	if (!snd_hctl_elem_read(mixerVolumeElem, mixerVolumeControl)) {
		/* assume left and right are the same */
		int x = snd_ctl_elem_value_get_integer(mixerVolumeControl, 0) * 100 / 127.0;
		if (x > 100)
			x = 100;
		return x;
	}

	return -1;
}

int Alsa::setCurrentVolumeLevel(int per)
{
	if (!hctl)
		initVolumeControl();

	int x = 127 * per / 100;

	snd_ctl_elem_value_set_integer(mixerVolumeControl, 0, x);
	snd_hctl_elem_write(mixerVolumeElem, mixerVolumeControl);
	snd_ctl_elem_value_set_integer(mixerVolumeControl, 1, x);
	snd_hctl_elem_write(mixerVolumeElem, mixerVolumeControl);

	return 0;
}
