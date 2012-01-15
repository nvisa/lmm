#include "mad.h"
#include "rawbuffer.h"
#define DEBUG
#include "emdesk/debug.h"

#include <mad.h>
#include <errno.h>

Mad::Mad(QObject *parent) :
	QObject(parent)
{
	stream = new mad_stream;
	frame = new mad_frame;
	synth = new mad_synth;
	mad_stream_init(stream);
	mad_frame_init(frame);
	mad_synth_init(synth);
}

int Mad::addBuffer(RawBuffer *buf)
{
	if (!buf)
		return -EINVAL;
	buffers.append(buf);
	return 0;
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

int Mad::decode()
{
	if (stream->buffer == NULL || stream->error == MAD_ERROR_BUFLEN) {
		if (buffers.size() == 0)
			return -ENOENT;
		RawBuffer *buf = buffers.takeFirst();
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
		outputBuffers << buf;
	}

	return 0;
}

RawBuffer * Mad::nextBuffer()
{
	if (outputBuffers.size() == 0)
		return NULL;
	return outputBuffers.takeFirst();
}
