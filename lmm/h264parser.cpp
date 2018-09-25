#include "h264parser.h"

#include <lmm/debug.h>

/* according to table 7-1 */
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

static inline uint32_t getBit(const uint8_t * const base, uint32_t offset)
{
	return ((*(base + (offset >> 0x3))) >> (0x7 - (offset & 0x7))) & 0x1;
}

static inline uint32_t getBits(const uint8_t * const base, uint32_t * const offset, uint8_t bits)
{
	uint32_t value = 0;
	for (int i = 0; i < bits; i++)
		value = (value << 1) | (getBit(base, (*offset)++) ? 1 : 0);
	return value;
}

/*
 * This function is stolen from the following stack entry:
 *
 *   http://stackoverflow.com/questions/2363500/does-anyone-have-an-easy-solution-to-parsing-exp-golomb-codes-using-c
 *
 */
static uint32_t getUeGolomb(const uint8_t * const base, uint32_t * const offset)
{
	uint32_t zeros = 0;

	// calculate zero bits. Will be optimized.
	while (0 == getBit(base, (*offset)++))
		zeros++;

	// insert first 1 bit
	uint32_t info = 1 << zeros;
	//(*offset)++;

	for (int32_t i = zeros - 1; i >= 0; i--)
		info |= getBit(base, (*offset)++) << i;

	return (info - 1);
}

static int32_t getSeGolomb(const uint8_t * const base, uint32_t * const offset)
{
	uint32_t zeros = 0;

	// calculate zero bits. Will be optimized.
	while (0 == getBit(base, (*offset)++))
		zeros++;

	// insert first 1 bit
	int32_t info = 1 << zeros;
	//(*offset)++;

	for (int32_t i = zeros - 1; i >= 0; i--)
		info |= getBit(base, (*offset)++) << i;

	/*reference: 9.1 Parsing process for Exp-Golomb codes*/
	if ((info - 1) &0x01)
		return info/2;
	else
		return -1 * (int)(info - 1) /2;
}

SimpleH264Parser::SimpleH264Parser(QObject *parent) :
	BaseLmmParser(parent)
{
	h264Mode = H264_OUTPUT_NALU;
	insertSpsPps = false;
	inputPacketized = true;
	extractSei = false;
	spsParsed = false;
	spsFps = 0;
	outputBufferNo = 0;
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
const uchar * SimpleH264Parser::findNextStartCode(const uchar *p, const uchar *end){
	const uint8_t *out= findNextStartCodeIn(p, end);
	if(p<out && out<end && !out[-1]) out--;
	return out;
}

int SimpleH264Parser::escapeEmulation(uchar *dst, const uchar *data, int size)
{
	int off = 0;
	int i;
	for (i = 0; i < size - 3; i++) {
		if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0x3) {
			if (data[i + 3] <= 0x3) {
				dst[off++] = data[i++];
				dst[off++] = data[i++];
				i++;
				dst[off++] = data[i];
				continue;
			}
		}

		dst[off++] = data[i];
	}
	while (i < size)
		dst[off++] = data[i++];

	return off;
}

void SimpleH264Parser::setExtractSei()
{
	extractSei = true;
}

H264SeiInfo SimpleH264Parser::getSeiData(int bufferNo)
{
	if (seiData.contains(bufferNo))
		return seiData.take(bufferNo);
	return H264SeiInfo();
}

H264SeiInfo SimpleH264Parser::parseNoStart(const uchar *data)
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

int SimpleH264Parser::processBuffer(const RawBuffer &buf)
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

	accbuf.append((const char *)buf.constData(), buf.size());

	QList<int> offsets;
	QList<int> nals;
	const uchar *encdata = (const uchar *)accbuf.constData();
	const uchar *encend = (const uchar *)accbuf.constData() + accbuf.size();
	const uchar *nal = SimpleH264Parser::findNextStartCode(encdata, encend);
	while (nal < encend - 4) {
		offsets << nal - encdata;
		int naltype = SimpleH264Parser::getNalType(nal);
		nals << naltype;
		nal = SimpleH264Parser::findNextStartCode(nal + 4, encend);
	}
	int total = 0;
	QList<RawBuffer> list;
	for (int i = 0; i < offsets.size() - 1; i++) {
		int start = offsets[i];
		int end = encend - encdata;
		if (i < offsets.size() - 1)
			end = offsets[i + 1];
		RawBuffer outbuf = RawBuffer("video/x-h264", end - start);
		outbuf.pars()->frameType = 1;
		int esize = end - start;//escapeEmulation((uchar *)outbuf.data(), encdata + start, end - start);
		outbuf.setUsedSize(esize);
		memcpy((uchar *)outbuf.data(), encdata + start, end - start);
		outbuf.pars()->h264NalType = nals[i];
		if (nals[i] == NAL_SPS) {
			if (!spsParsed) {
				sps_t sps = parseSps((const uchar *)outbuf.constData());
				float fps = (float)sps.vui.timescale / sps.vui.num_unit_in_ticks / 2;
				if (fps > 0) {
					spsParsed = true;
					spsFps = fps;
				}
			}
		} else if (nals[i] == NAL_SLICE_IDR) {
			outbuf.pars()->frameType = 0;
		}
		outbuf.pars()->duration = 0;
		if (spsParsed && (nals[i] == NAL_SLICE_IDR || nals[i] == NAL_SLICE))
			outbuf.pars()->duration = 1000 / spsFps;
		outbuf.pars()->metaData = buf.constPars()->metaData;
		outbuf.pars()->streamBufferNo = outputBufferNo++;
		outbuf.pars()->captureTime = buf.constPars()->captureTime;
		newOutputBuffer(0, outbuf);
		total += outbuf.size();
	}
	if (offsets.size())
		accbuf = accbuf.mid(offsets.takeLast());
	//if (buf.size() != total)
		/* TODO : Add support for data accumulation */
		//mDebug("input/output buffer size mismatch (%d != %d), accumulation is not supported!", total, buf.size());
	return newOutputBuffer(0, list);
}

void SimpleH264Parser::extractSeiData(RawBuffer buf)
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

int SimpleH264Parser::getNalType(const uchar *data)
{
	return data[4] & 0x1f;
}

SimpleH264Parser::sps_t SimpleH264Parser::parseSps(const uchar *data)
{
	sps_t sps;
	int i;
	/* parsing according to 7.3.2 */
	uint32_t off = 5 * 8;

	sps.profile_idc = getBits(data, &off, 8);
	sps.constraint_set0_5_flag = getBits(data, &off, 6);
	sps.reserved_zero_2bits = getBits(data, &off, 2);
	sps.level_idc = getBits(data, &off, 8);
	sps.seq_parameter_set_id = getUeGolomb(data, &off);
	if( sps.profile_idc == 100 || sps.profile_idc == 110 ||
			sps.profile_idc == 122 || sps.profile_idc == 144 ) {
		sps.chroma_format_idc = getUeGolomb(data, &off);
		if( sps.chroma_format_idc == 3 )
			sps.residual_colour_transform_flag = getBits(data, &off, 1);
		sps.bit_depth_luma_minus8 = getUeGolomb(data, &off);
		sps.bit_depth_chroma_minus8 = getUeGolomb(data, &off);
		sps.qpprime_y_zero_transform_bypass_flag = getBits(data, &off, 1);
		sps.seq_scaling_matrix_present_flag = getBits(data, &off, 1);
		if( sps.seq_scaling_matrix_present_flag )
			for( i = 0; i < 8; i++ ) {
				sps.seq_scaling_list_present_flag[ i ] = getBits(data, &off, 1);
				if( sps.seq_scaling_list_present_flag[ i ] ) {
					int j;
					int lastScale = 8;
					int nextScale = 8;
					if( i < 6 ) {
						for( j = 0; j < 16; j++ ) {
							if( nextScale != 0 ) {
								int delta_scale = getSeGolomb(data, &off);
								nextScale = ( lastScale + delta_scale + 256 ) % 256;
								sps.UseDefaultScalingMatrix4x4Flag[i] = ( j == 0 && nextScale == 0 ) ? 1:0 ;
							}
							sps.ScalingList4x4[j] = ( nextScale == 0 ) ? lastScale : nextScale;
							lastScale = sps.ScalingList4x4[ j ];
						}
					} else {
						for( j = 0; j < 64; j++ ) {
							if( nextScale != 0 ) {
								int delta_scale = getSeGolomb(data, &off);
								nextScale = ( lastScale + delta_scale + 256 ) % 256;
								sps.UseDefaultScalingMatrix8x8Flag[i] = ( j == 0 && nextScale == 0 ) ? 1:0 ;
							}
							sps.ScalingList8x8[j] = ( nextScale == 0 ) ? lastScale : nextScale;
							lastScale = sps.ScalingList8x8[ j ];
						}
					}
				}
			}
	}
	sps.log2_max_frame_num_minus4 = getUeGolomb(data, &off);
	sps.pic_order_cnt_type = getUeGolomb(data, &off);
	if( sps.pic_order_cnt_type == 0 )
		sps.log2_max_pic_order_cnt_lsb_minus4 = getUeGolomb(data, &off);
	else if( sps.pic_order_cnt_type == 1 ) {
		sps.delta_pic_order_always_zero_flag = getBits(data, &off, 1);
		sps.offset_for_non_ref_pic = getSeGolomb(data, &off);
		sps.offset_for_top_to_bottom_field = getSeGolomb(data, &off);
		sps.num_ref_frames_in_pic_order_cnt_cycle = getUeGolomb(data, &off);
		for( i = 0; i < sps.num_ref_frames_in_pic_order_cnt_cycle; i++ )
			sps.offset_for_ref_frame[ i ] = getSeGolomb(data, &off);
	}
	sps.num_ref_frames = getUeGolomb(data, &off);
	sps.gaps_in_frame_num_value_allowed_flag = getBits(data, &off, 1);
	sps.pic_width_in_mbs_minus1 = getUeGolomb(data, &off);
	sps.pic_height_in_map_units_minus1 = getUeGolomb(data, &off);
	sps.frame_mbs_only_flag = getBits(data, &off, 1);
	if( !sps.frame_mbs_only_flag )
		sps.mb_adaptive_frame_field_flag = getBits(data, &off, 1);

	sps.direct_8x8_inference_flag = getBits(data, &off, 1);
	sps.frame_cropping_flag = getBits(data, &off, 1);
	if( sps.frame_cropping_flag ) {
		sps.frame_crop_left_offset = getUeGolomb(data, &off);
		sps.frame_crop_right_offset = getUeGolomb(data, &off);
		sps.frame_crop_top_offset = getUeGolomb(data, &off);
		sps.frame_crop_bottom_offset = getUeGolomb(data, &off);
	}
	sps.vui_parameters_flags = getBits(data, &off, 1);
	if (sps.vui_parameters_flags) {
		sps.vui.aspect_ratio_present = getBits(data, &off, 1);
		if (sps.vui.aspect_ratio_present) {
			sps.vui.aspect_idc = getBits(data, &off, 8);
			if (sps.vui.aspect_idc == 255) {
				sps.vui.sar_w = getBits(data, &off, 16);
				sps.vui.sar_h = getBits(data, &off, 16);
			}
		}
		sps.vui.overscan_present = getBits(data, &off, 1);
		if (sps.vui.overscan_present)
			sps.vui.overscan_approp = getBits(data, &off, 1);
		sps.vui.video_signal_type_present = getBits(data, &off, 1);
		if (sps.vui.video_signal_type_present) {
			getBits(data, &off, 4);
			if (getBits(data, &off, 1)) {
				getBits(data, &off, 24);
			}
		}
		if (getBits(data, &off, 1)) {
			/* chroma loc */
			fDebug("Unsupported SPS format, sorry");
			return sps;
		}
		sps.vui.timing_info_present = getBits(data, &off, 1);
		if (sps.vui.timing_info_present) {
			sps.vui.num_unit_in_ticks = getBits(data, &off, 32);
			sps.vui.timescale = getBits(data, &off, 32);
			sps.vui.fixed_frame_rate = getBits(data, &off, 1);
		}
	}

	return sps;
}
