#include "mad.h"
#include "rawbuffer.h"
#define DEBUG
#include "emdesk/debug.h"

#include <mad.h>
#include <errno.h>
#include <taglib/taglib.h>
#include <taglib/id3v2header.h>
#include <taglib/id3v2tag.h>

Mad::Mad(QObject *parent) :
	BaseLmmDecoder(parent)
{
}

static signed short madToShort(mad_fixed_t Fixed)
{
	/* A fixed point number is formed of the following bit pattern:
	 *
	 * SWWWFFFFFFFFFFFFFFFFFFFFFFFFFFFF
	 * MSB                          LSB
	 * S ==> Sign (0 is positive, 1 is negative)
	 * W ==> Whole part bits
	 * F ==> Fractional part bits
	 *
	 * This pattern contains MAD_F_FRACBITS fractional bits, one
	 * should alway use this macro when working on the bits of a fixed
	 * point number. It is not guaranteed to be constant over the
	 * different platforms supported by libmad.
	 *
	 * The signed short value is formed, after clipping, by the least
	 * significant whole part bit, followed by the 15 most significant
	 * fractional part bits. Warning: this is a quick and dirty way to
	 * compute the 16-bit number, madplay includes much better
	 * algorithms.
	 */

	/* Clipping */
	if(Fixed>=MAD_F_ONE)
		return(SHRT_MAX);
	if(Fixed<=-MAD_F_ONE)
		return(-SHRT_MAX);

	/* Conversion. */
	Fixed=Fixed>>(MAD_F_FRACBITS-15);
	return((signed short)Fixed);
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

int Mad::decodeAll()
{
	RawBuffer *buf = NULL;
	while (inputBuffers.size()) {
		/* TODO: free finished buffers */
		if (buf) {
			if (stream->next_frame) {
				size_t left = stream->bufend - stream->next_frame;
				madBuffer.prepend((const char *)stream->next_frame, left);
			}
		}
		buf = inputBuffers.takeFirst();
		if (!buf)
			return -ENOENT;
		madBuffer.append((const char *)buf->data(), buf->size());
		handleInputTimeStamps(buf);
		delete buf;
		buf = NULL;
		mad_stream_buffer(stream, (const unsigned char *)madBuffer.constData(), madBuffer.size());
		if (mad_header_decode(&frame->header, stream) == -1) {
			if (stream->error == MAD_ERROR_BUFLEN) {
				mInfo("not enough data for header");
				continue;
			}
			mDebug("mad header decode error: %s", mad_stream_errorstr(stream));
		}
		mInfo("decoding...");
		if (mad_frame_decode(frame, stream) == -1) {
			if (stream->error == MAD_ERROR_BUFLEN) {
				if (stream->next_frame == (const unsigned char *)madBuffer.constData())
					continue;
				mDebug("sync error");
				continue;//goto next_no_samples;
			} else if (stream->error == MAD_ERROR_BADDATAPTR)
				continue;//goto next_no_samples;
			/* handle errors */
			mDebug("mad frame decode error handling");
			if (!MAD_RECOVERABLE(stream->error))
				return -EIO;
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
			}
			mad_frame_mute(frame);
			mad_synth_mute(synth);
			const unsigned char *before = stream->ptr.byte;
			if (mad_stream_sync(stream))
				mDebug("syncing failed");
			const unsigned char *after = stream->ptr.byte;
			mDebug("diff between syncs are %d", after - before);
			mad_stream_sync(stream);
			continue;//goto next_no_samples;
		}
		int nSamples = MAD_NSBSAMPLES(&frame->header) * 32;
		RawBuffer *outbuf = new RawBuffer;
		outbuf->setSize(nSamples * 2 * 2);
		mInfo("synthing %d...", outbuf->size());
		mad_synth_frame(synth, frame);
		const mad_fixed_t *leftCh = synth->pcm.samples[0];
		const mad_fixed_t *rightCh = synth->pcm.samples[1];
		short *out = (short *)outbuf->data();
		int count = nSamples;
		while (count--) {
			*out++ = scale(*leftCh++) & 0xffff;
			*out++ = scale(*rightCh++) & 0xffff;
		}
		setOutputTimeStamp(outbuf);
		outputBuffers << outbuf;

		int cons = stream->next_frame - (unsigned char *)madBuffer.constData();
		mInfo("mad consumed %d bytes", cons);
		if (cons > 0)
			madBuffer.remove(0, cons);
	}
	return 0;
}

int Mad::start()
{
	stream = new mad_stream;
	frame = new mad_frame;
	synth = new mad_synth;
	mad_stream_init(stream);
	mad_frame_init(frame);
	mad_synth_init(synth);
	return 0;
}

int Mad::stop()
{
	mad_synth_finish(synth);
	mad_frame_finish(frame);
	mad_stream_finish(stream);
	delete synth;
	delete frame;
	delete stream;
	return 0;
}

int Mad::decode()
{
	if (stream->buffer == NULL || stream->error == MAD_ERROR_BUFLEN) {
		if (inputBuffers.size() == 0)
			return -ENOENT;
		RawBuffer *buf = inputBuffers.takeFirst();
		if (stream->next_frame) {
			size_t left = stream->bufend - stream->next_frame;
			if (buf->prepend(stream->next_frame, left))
				mDebug("*** prepend error ***");
		}
		if (MAD_BUFFER_MDLEN < buf->size())
			mDebug("chunk size is too big for mad decoder");
		/* Pipe the new buffer content to libmad's stream decoder facility. */
		mad_stream_buffer(stream, (const unsigned char *)buf->data(), buf->size());
		stream->error = MAD_ERROR_NONE;
		handleInputTimeStamps(buf);
		delete buf;
	}
	if (mad_frame_decode(frame, stream)) {
		if(MAD_RECOVERABLE(stream->error)) {
			mDebug("recoverable frame level error (%s)", mad_stream_errorstr(stream));
			return 0;
		} else {
			if(stream->error == MAD_ERROR_BUFLEN)
				return 0;
			mDebug("unrecoverable frame level error (%s).", mad_stream_errorstr(stream));
			return -EIO;
		}
	}
	mad_synth_frame(synth, frame);
	if (synth->pcm.length) {
		RawBuffer *buf = new RawBuffer;
		buf->setSize(synth->pcm.length * 4);
		short *data = (short *)buf->data();
		for (int i = 0; i < synth->pcm.length; ++i) {
			signed short lsample, rsample;

			lsample = madToShort(synth->pcm.samples[0][i]);
			rsample = madToShort(synth->pcm.samples[1][i]);
			data[i * 2] = lsample;
			data[i * 2 + 1] = rsample;
		}
		setOutputTimeStamp(buf);
		outputBuffers << buf;
	}

	return 0;
}

