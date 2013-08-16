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

H264Parser::H264Parser(QObject *parent) :
	BaseLmmParser(parent)
{
	h264Mode = H264_OUTPUT_NALU;
	packState = 0;
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
					outputLock.lock();
					outputBuffers << buf2;
					mInfo("new nal packet with size %d %d", packSize, i);
					releaseOutputSem(0);
					outputLock.unlock();
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
