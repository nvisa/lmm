#ifndef SIMPLEH264PARSER_H
#define SIMPLEH264PARSER_H

#include "baselmmparser.h"

class H264SeiInfo {
public:
	H264SeiInfo()
	{
		size = 0;
		offset = -1;
		type = -1;
	}

	int size;
	int offset;
	int type;
	QByteArray payload;
};

class SimpleH264Parser : public BaseLmmParser
{
	Q_OBJECT
public:
	enum H264NalType {
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
	struct sps_t
	{
		int profile_idc;
		int constraint_set0_5_flag;
		int reserved_zero_2bits;
		int level_idc;
		int seq_parameter_set_id;
		int chroma_format_idc;
		int residual_colour_transform_flag;
		int bit_depth_luma_minus8;
		int bit_depth_chroma_minus8;
		int qpprime_y_zero_transform_bypass_flag;
		int seq_scaling_matrix_present_flag;
		int seq_scaling_list_present_flag[8];
		int ScalingList4x4[6];
		int UseDefaultScalingMatrix4x4Flag[6];
		int ScalingList8x8[2];
		int UseDefaultScalingMatrix8x8Flag[2];
		int log2_max_frame_num_minus4;
		int pic_order_cnt_type;
		int log2_max_pic_order_cnt_lsb_minus4;
		int delta_pic_order_always_zero_flag;
		int offset_for_non_ref_pic;
		int offset_for_top_to_bottom_field;
		int num_ref_frames_in_pic_order_cnt_cycle;
		int offset_for_ref_frame[256];
		int num_ref_frames;
		int gaps_in_frame_num_value_allowed_flag;
		int pic_width_in_mbs_minus1;	//resolution
		int pic_height_in_map_units_minus1;//resolution
		int frame_mbs_only_flag;		//resolution
		int mb_adaptive_frame_field_flag;
		int direct_8x8_inference_flag;
		int frame_cropping_flag;		//resolution
		int frame_crop_left_offset;		//resolution
		int frame_crop_right_offset;	//resolution
		int frame_crop_top_offset;		//resolution
		int frame_crop_bottom_offset;	//resolution
		int vui_parameters_flags;
		struct _vui {
			bool aspect_ratio_present;
			uchar aspect_idc;
			ushort sar_w;
			ushort sar_h;
			bool overscan_present;
			bool overscan_approp;
			bool video_signal_type_present;
			bool timing_info_present;
			uint num_unit_in_ticks;
			uint timescale;
			bool fixed_frame_rate;

		};
		_vui vui;
	};

	explicit SimpleH264Parser(QObject *parent = 0);
	static const uchar * findNextStartCode(const uchar *p, const uchar *end);
	static int escapeEmulation(uchar *dst, const uchar *data, int size);
	static int getNalType(const uchar *data);
	static sps_t parseSps(const uchar *data);
	void setSpsPpsInsertion(bool v) { insertSpsPps = v; }
	void setInputPacketized(bool v) { inputPacketized = v; }
	void setExtractSei();
	H264SeiInfo getSeiData(int bufferNo);
signals:
	
public slots:
protected:
	enum h264_output {
		H264_OUTPUT_NALU,
		H264_OUTPUT_AU,
	};
	H264SeiInfo parseNoStart(const uchar *data);
	int processBuffer(const RawBuffer &buf);
	void extractSeiData(RawBuffer buf);

	h264_output h264Mode;
	RawBuffer idrBuf;
	RawBuffer spsBuf;
	RawBuffer ppsBuf;
	bool insertSpsPps;
	bool inputPacketized;
	bool extractSei;
	QMap<int, H264SeiInfo> seiData;
	float spsFps;
	bool spsParsed;
	QByteArray accbuf;
	int outputBufferNo;
};

#endif // SIMPLEH264PARSER_H
