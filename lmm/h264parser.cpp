#include "h264parser.h"

#include <lmm/debug.h>
#include <lmm/circularbuffer.h>

enum h264_nal_type
{
	NAL_UNKNOWN = 0,
	NAL_SLICE = 1,
	NAL_SLICE_DPA = 2,
	NAL_SLICE_DPB = 3,
	NAL_SLICE_DPC = 4,
	NAL_SLICE_IDR = 5,
	NAL_SEI = 6,
	NAL_SPS = 7,
	NAL_PPS = 8,
	NAL_AU_DELIMITER = 9,
	NAL_SEQ_END = 10,
	NAL_STREAM_END = 11,
	NAL_FILTER_DATA = 12
};

enum h264_nal_sei_type
{
	NAL_SEI_BUFFERING_PERIOD,
	NAL_SEI_PIC_TIMING,
	NAL_SEI_PAN_SCAN_RECT,
	NAL_SEI_FILLER_PAYLOAD,
	NAL_SEI_USER_DATA_ITU_T_35,
	NAL_SEI_USER_DATA_UNREGISTERED,
	NAL_SEI_RECOVERY_POINT,
	NAL_SEI_DEC_REF_PIC_MARKING_REPETION,
	NAL_SEI_SPARE_PIC,
	NAL_SEI_SCENE_INFO,
	NAL_SEI_SUB_SEQ_INFO,
	NAL_SEI_FULL_FRAME_FREEZE,
	NAL_SEI_FULL_FRAME_FREEZE_RELEASE,
	NAL_SEI_FULL_FRAME_FREEZE_SNAPSHOT,
	NAL_SEI_FULL_PROGRESSIVE_REFINEMENT_SEGMENT_START,
	NAL_SEI_FULL_PROGRESSIVE_REFINEMENT_SEGMENT_END,
	NAL_SEI_FULL_MOTION_CONSTRAINED_SLICE_GROUP_SET
};

H264Parser::H264Parser(QObject *parent) :
	BaseLmmParser(parent)
{
	h264Mode = H264_OUTPUT_NALU;
	packState = 0;
	insertSpsPps = false;
	inputPacketized = true;
	extractSei = false;
}

static const uint8_t * findNextStartCodeIn(const uint8_t *p, const uint8_t *end)
{
	const uint8_t *a = p + 4 - ((intptr_t)p & 3);

	for (end -= 3; p < a && p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	for (end -= 3; p < end; p += 4) {
		uint32_t x = *(const uint32_t*)p;
		//      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
		//      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
		if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
			if (p[1] == 0) {
				if (p[0] == 0 && p[2] == 1)
					return p;
				if (p[2] == 0 && p[3] == 1)
					return p+1;
			}
			if (p[3] == 0) {
				if (p[2] == 0 && p[4] == 1)
					return p+2;
				if (p[4] == 0 && p[5] == 1)
					return p+3;
			}
		}
	}

	for (end += 3; p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	return end + 3;
}

/**
 * @brief H264Parser::findNextStartCode
 * @param p
 * @param end
 * @return
 *
 * This function is taken from FFmpeg libraries, it detects next H.264
 * nal start code in the given buffer and returns the beginning of the buffer.
 * Return pointer contains the start code as well. If it cannot find start
 * code it returns the end of the given buffer minus 4.
 */
const uchar * H264Parser::findNextStartCode(const uchar *p, const uchar *end){
	const uint8_t *out= findNextStartCodeIn(p, end);
	if(p<out && out<end && !out[-1]) out--;
	return out;
}

void H264Parser::setExtractSei()
{
	extractSei = true;
}

H264SeiInfo H264Parser::getSeiData(int bufferNo)
{
	if (seiData.contains(bufferNo))
		return seiData.take(bufferNo);
	return H264SeiInfo();
}

int H264Parser::parse(const uchar *data, int size)
{
	if (packState == 0)
		return findNextStartCode(data, size);

	for (int i = 4; i < size - 2; i++) {
		if (packState == 1) {
			if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
				/* parsed a complete nal unit */
				RawBuffer buf2("video/x-h264", packSize);
				memcpy(buf2.data(), data, packSize);
				if (h264Mode == H264_OUTPUT_NALU) {
					newOutputBuffer(0, buf2);
					mInfo("new nal packet with size %d %d", packSize, i);
				} else {
					nalBuffers << buf2;
				}
				circBuf->lock();
				circBuf->useData(packSize);
				mInfo("using circular buffer data: %d", circBuf->freeSize());
				circBuf->unlock();
				packSize = 4;
				break;
			} else {
				packSize++;
			}
		}
	}

	if (h264Mode == H264_OUTPUT_AU) {
		while (nalBuffers.size()) {
			RawBuffer buf = nalBuffers.takeFirst();
			const uchar *data = (const uchar *)buf.constData();
			//int nal_ref_idc = (data[3] & 0x60) >> 5;
			h264_nal_type nal = h264_nal_type(data[3] & 0x1f);
			//qDebug() << nal << nal_ref_idc;
			au << buf;
			switch (nal) {
			case NAL_SLICE:
			case NAL_SLICE_DPA:
			case NAL_SLICE_DPB:
			case NAL_SLICE_DPC:
			case NAL_SLICE_IDR:
				break;
			case NAL_AU_DELIMITER: {
				QMap<int, int> cnt;
				for (int i = 0; i < au.size(); i++) {
					buf = au.at(i);
					data = (const uchar *)buf.constData();
					cnt[data[3] & 0x1f] += 1;
				}
				qDebug() << "new access unit" << au.size() << cnt;
				au.clear();
				break;
			}
			case NAL_UNKNOWN:
			case NAL_SEI:
			case NAL_SPS:
			case NAL_PPS:
			case NAL_SEQ_END:
			case NAL_STREAM_END:
			case NAL_FILTER_DATA:
			default:
				break;
			};

		}
	}
	return 0;
}

H264SeiInfo H264Parser::parseNoStart(const uchar *data)
{
	H264SeiInfo sinfo;
	sinfo.offset = -1;
	int pos = 0;
	uchar nalu = data[pos] & 0x1f;
	pos += 1;
	//parsing according to 7.3.2.3.1
	if (nalu == 6) {
		sinfo.offset = pos - 1;
		int type = 0, size = 0;
		while (data[pos] == 255)
			type += data[pos++];
		type += data[pos++];
		while (data[pos] == 255)
			size += data[pos++];
		size += data[pos++];
		sinfo.size = size;
		sinfo.type = type;
		sinfo.payload.append(nalu);
		sinfo.payload.append(type);
		sinfo.payload.append(size & 0xff);
		sinfo.payload.append(size >> 8);
		while (size) {
			sinfo.payload.append(data[pos++]);
			size--;
		}
	}
	return sinfo;
}

int H264Parser::processBuffer(const RawBuffer &buf)
{
	if (insertSpsPps && spsBuf.size() == 0) {
		const uchar *d = (const uchar *)buf.constData();
		while (d) {
			int nal = d[4] & 0x1f;
			int next = findNextStartCode(&d[4], d + buf.size()) - d;
			if (nal != NAL_SLICE)
				mInfo("nal %d", nal);
			if (nal == NAL_SLICE_IDR) {
				idrBuf = RawBuffer("video/x-h264", d, next);
			} else if (nal == NAL_SPS) {
				spsBuf = RawBuffer("video/x-h264", d, next);
			} else if (nal == NAL_PPS) {
				ppsBuf = RawBuffer("video/x-h264", d, next);
			}
			if (next < buf.size())
				d = d + next;
			else
				break;
		}
	}

	if (insertSpsPps && buf.constPars()->frameType == 0) {
		mInfo("inserting SPS and PPS");
		RawBuffer buf2("video/x-h264", buf.size() + spsBuf.size() + ppsBuf.size());
		buf2.setParameters(buf.constPars());
		char *d = (char *)buf2.data();
		memcpy(d, spsBuf.constData(), spsBuf.size());
		d += spsBuf.size();
		memcpy(d, ppsBuf.constData(), ppsBuf.size());
		d += ppsBuf.size();
		memcpy(d, buf.constData(), buf.size());
		return newOutputBuffer(0, buf2);
	}

	if (extractSei)
		extractSeiData(buf);

	if (inputPacketized)
		return newOutputBuffer(0, buf);

	const uchar *d = (const uchar *)buf.constData();
	const uchar *start = (const uchar *)buf.constData();
	int out = 0;
	while (d) {
		int nal = d[4] & 0x1f;
		int next = findNextStartCode(&d[4], d + buf.size()) - d;
		if (nal != NAL_SLICE)
			qDebug("nal %d", nal);
		if (d + next < start + buf.size()) {
			RawBuffer buf2 = RawBuffer("video/x-h264", d, next);
			buf2.pars()->h264NalType = nal;
			newOutputBuffer(0, buf2);
			d = d + next;
			out += next;
		} else {
			RawBuffer buf2 = RawBuffer("video/x-h264", d, start - d + buf.size());
			buf2.pars()->h264NalType = nal;
			newOutputBuffer(0, buf2);
			out += start - d + buf.size();
			break;
		}
	}
	if (out != buf.size()) {
		mDebug("there was an error in parsing, in %d bytes, out %d bytes", buf.size(), out);
	}
	//newOutputBuffer(0, buf);
	return 0;
}

void H264Parser::extractSeiData(RawBuffer buf)
{
	const uchar *start = (const uchar *)buf.constData();
	const uchar *d = start;
	while (d) {
		int nal = d[4] & 0x1f;
		int next = findNextStartCode(&d[4], d + buf.size()) - d;
		if (nal == NAL_SEI) {
			H264SeiInfo si = parseNoStart(d + 4);
			if (h264_nal_sei_type(si.type) == NAL_SEI_USER_DATA_UNREGISTERED)
				seiData.insert(buf.pars()->streamBufferNo, si);
		}
		if (d + next < start + buf.size()) {
			d = d + next;
		} else {
			break;
		}
	}
}

int H264Parser::findNextStartCode(const uchar *data, int size)
{
	for (int i = 0; i < size - 2; i++) {
		if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
			packSize = 4;
			packState = 1;
			mDebug("found new start code at %d", i);
			circBuf->lock();
			circBuf->useData(i);
			circBuf->unlock();
			break;
		}
	}
	return 0;
}

int H264Parser::getNalType(const uchar *data)
{
	return data[4] & 0x1f;
}
