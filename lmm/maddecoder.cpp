#include "maddecoder.h"
#include "rawbuffer.h"
#define DEBUG
#include "debug.h"

#include <mad.h>
#include <errno.h>
#include <taglib/taglib.h>
#include <taglib/id3v2header.h>
#include <taglib/id3v2tag.h>

#include <QSemaphore>

MadDecoder::MadDecoder(QObject *parent) :
	BaseLmmDecoder(parent)
{
	stream = NULL;
	frame = NULL;
	synth = NULL;
}

static inline qint16 scale(mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - 16));

	/* clip */
	if (sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if (sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

int MadDecoder::decodeAll()
{
	while (inputBuffers.size()) {
		decode();
	}

	return 0;
}

int MadDecoder::flush()
{
	mDebug("flushing");
	if (frame)
		mad_frame_mute(frame);
	if (synth)
		mad_synth_mute(synth);
	madBuffer.clear();
	return BaseLmmDecoder::flush();
}

int MadDecoder::startDecoding()
{
	stream = new mad_stream;
	frame = new mad_frame;
	synth = new mad_synth;
	mad_stream_init(stream);
	mad_frame_init(frame);
	mad_synth_init(synth);
	decodeCount = 0;
	return 0;
}

int MadDecoder::stopDecoding()
{
	mad_synth_finish(synth);
	mad_frame_finish(frame);
	mad_stream_finish(stream);
	delete synth;
	delete frame;
	delete stream;
	stream = NULL;
	frame = NULL;
	synth = NULL;
	madBuffer.clear();
	return 0;
}

int MadDecoder::decode()
{
	RawBuffer buf;
	if (buf.size()) {
		if (stream->next_frame) {
			size_t left = stream->bufend - stream->next_frame;
			madBuffer.prepend((const char *)stream->next_frame, left);
		}
	}
	inputLock.lock();
	buf = inputBuffers.takeFirst();
	inputLock.unlock();
	if (!buf.size())
		return -ENOENT;
	madBuffer.append((const char *)buf.data(), buf.size());
	mad_stream_buffer(stream, (const unsigned char *)madBuffer.constData(), madBuffer.size());
	if (mad_header_decode(&frame->header, stream) == -1) {
		if (stream->error == MAD_ERROR_BUFLEN) {
			mInfo("not enough data for header");
			return -1;
		}
		mDebug("mad header decode error: %s", mad_stream_errorstr(stream));
	}
	mInfo("decoding...");
	if (mad_frame_decode(frame, stream) == -1) {
		if (stream->error == MAD_ERROR_BUFLEN) {
			if (stream->next_frame == (const unsigned char *)madBuffer.constData())
				return -2;
			mDebug("sync error");
			return -3;//goto next_no_samples;
		} else if (stream->error == MAD_ERROR_BADDATAPTR) {
			mDebug("not enough data, available is %d", madBuffer.size());
			if (madBuffer.size() > 2500)
				madBuffer.clear();
			return -4;//goto next_no_samples;
		}
		/* handle errors */
		mInfo("mad frame decode error handling, error is %d", stream->error);
		if (!MAD_RECOVERABLE(stream->error)) {
			mDebug("unrecoverable stream error");
			return -EIO;
		}
		if (stream->error == MAD_ERROR_LOSTSYNC) {
			TagLib::ByteVector vector((const char *)stream->this_frame, stream->bufend - stream->this_frame);
			TagLib::ID3v2::Header header(vector);
			int tagSize = header.tagSize();//id3_tag_query(stream->this_frame, stream->bufend - stream->this_frame);
			if (tagSize > madBuffer.size()) {
				mDebug("skipping partial id3 tag");
			} else if (tagSize > 0) {
				//struct id3_tag *tag;
				//const id3_byte_t *data;
				mad_stream_skip(stream, tagSize - 1);
				//tag = id3_tag_parse(stream->this_frame, tagSize);
				/*if (tag) {

				}*/
			}
		} else
			flush();
		mad_frame_mute(frame);
		mad_synth_mute(synth);
		const unsigned char *before = stream->ptr.byte;
		if (mad_stream_sync(stream))
			mDebug("syncing failed");
		const unsigned char *after = stream->ptr.byte;
		mDebug("diff between syncs are %d", after - before);
		mad_stream_sync(stream);
		return -5;//goto next_no_samples;
	}
	int nSamples = MAD_NSBSAMPLES(&frame->header) * 32;
	RawBuffer outbuf("audio/raw-int", nSamples * 2 * 2);
	mInfo("synthing %d...", outbuf.size());
	mad_synth_frame(synth, frame);
	const mad_fixed_t *leftCh = synth->pcm.samples[0];
	const mad_fixed_t *rightCh = synth->pcm.samples[1];
	short *out = (short *)outbuf.data();
	int count = nSamples;
	while (count--) {
		*out++ = scale(*leftCh++) & 0xffff;
		*out++ = scale(*rightCh++) & 0xffff;
	}
	outbuf.setPts(buf.getPts());
	outbuf.setDuration(buf.getDuration());
	outbuf.setStreamBufferNo(decodeCount++);
	outputLock.lock();
	outputBuffers << outbuf;
	releaseOutputSem(0);
	outputLock.unlock();

	int cons = stream->next_frame - (unsigned char *)madBuffer.constData();
	mInfo("mad consumed %d bytes", cons);
	if (cons > 0)
		madBuffer.remove(0, cons);
	return 0;
}

int MadDecoder::decodeBlocking()
{
	if (!acquireInputSem(0))
		return -EINVAL;
	return decode();
}

