#include "tsdemux.h"
#include "emdesk/debug.h"

#define MAX_PES_PAYLOAD 200*1024

struct ts_header {
	char sync_byte;
	char transport_error_indicator;
	char payload_unit_start_indicator;
	char transport_priority;
	short PID;
	char transport_scrambling_control;
	char adaptation_field_control;
	char continuity_counter;
};

struct pes_header_data {
	unsigned char PTS_DTS_flags;
	unsigned char ESCR_flag;
	unsigned char ES_rate_flag;
	unsigned char DSM_trick_mode_flag;
	unsigned char additional_copy_info_flag;
	unsigned char PES_CRC_flag;
	unsigned char PES_extension_flag;
	unsigned char PES_header_data_length;
};

struct pes_data {
	pes_header_data header;
	int packet_start_code_prefix;
	unsigned char stream_id;
	unsigned short PES_packet_length;
};

struct ts_program {
	short PID;
	int ccn;
	unsigned char payload[MAX_PES_PAYLOAD];
	int payloadLen;
	pes_data pes;
	bool foundStart;
};

static void fill_header(struct ts_header *h, unsigned char *src)
{
	h->sync_byte = src[0];
	h->transport_error_indicator = (src[1] >> 7) & 1;
	h->payload_unit_start_indicator = (src[1] >> 6) & 1;
	h->transport_priority = (src[1] >> 5) & 1;
	h->PID = src[2] + ((src[1] & 0x1f) << 8);
	h->transport_scrambling_control = (src[3] >> 6) & 3;
	h->adaptation_field_control = (src[3] >> 4) & 3;
	h->continuity_counter = src[3] & 0xf;
}

tsDemux::tsDemux(QObject *parent) :
	QObject(parent)
{
	memset(programs, 0, 8192);
}

static void parse_pes_header(pes_data *pes, unsigned char *buf)
{
	pes_header_data *h = &pes->header;
	h->PTS_DTS_flags = buf[1] & 0xc0;
	h->ESCR_flag = buf[1] & 0x20;
	h->ES_rate_flag = buf[1] & 0x10;
	h->DSM_trick_mode_flag = buf[1] & 0x8;
	h->additional_copy_info_flag = buf[1] & 0x4;
	h->PES_CRC_flag = buf[1] & 0x2;
	h->PES_extension_flag = buf[1] & 0x1;
	h->PES_header_data_length = buf[2] + 9;
}

int tsDemux::newData(unsigned char *data)
{
	int err = 0;
	int esSize;
	pes_data *pes;
	struct ts_header header;
	fill_header(&header, &data[0]);
	if (header.sync_byte != 0x47) {
		mDebug("sync error");
		return -1;
	}
	if (header.PID != 512)
		return -1;
	if (programs[header.PID] == 0) {
		ts_program *prog = new ts_program;
		programs[header.PID] = prog;
		programs[header.PID]->payloadLen = 0;
		prog->foundStart = false;
	}
	ts_program *prog = programs[header.PID];
	if (prog->ccn != ((header.continuity_counter - 1) & 0xf))
		mDebug("continuity jump for PID %d %d %d", header.PID, header.continuity_counter, prog->ccn);

	int dataBytes = 184;
	int afLen = 0;
	unsigned char *pesData;
	if (header.adaptation_field_control == 0x10) {
		mDebug("adaption field only data, skipping");
		goto out;
	} else if (header.adaptation_field_control == 0x11) {
		int afLen = data[4];
		dataBytes -= afLen + 1;
		pesData = &data[5 + afLen];
	} else
		pesData = &data[4];
	mInfo("AF len=%d dataBytes=%d", afLen, dataBytes);
	pes = &prog->pes;
	if (header.payload_unit_start_indicator) {
		if (prog->foundStart) {
			err = 1;
			goto out;
		}
		prog->payloadLen = 0;
		prog->foundStart = 1;
		pes->packet_start_code_prefix = (pesData[2] << 0) + (pesData[1] << 8) + (pesData[0] << 16);
		pes->stream_id = pesData[3];
		pes->PES_packet_length = (pesData[5] << 8) + pesData[4];
		/*
		 * Let's parse PES header, note that this is dependent upon stream_id which we ignore ATM
		 * TODO: Implement check on stream_id
		 */
		parse_pes_header(pes, pesData + 6);
	} else if (!prog->foundStart)
		goto out;

	esSize = dataBytes - pes->header.PES_header_data_length;
	if (esSize <= 0) {
		pes->header.PES_header_data_length -= dataBytes;
		goto out;
	}
	memcpy(prog->payload + prog->payloadLen, pesData + pes->header.PES_header_data_length, esSize);
	prog->payloadLen += esSize;
	pes->header.PES_header_data_length = 0;

out:
	if (err != 1)
		prog->ccn = header.continuity_counter;
	return err;
}

const char * tsDemux::getStream(int pid, int *size)
{
	ts_program *prog = programs[pid];
	if (!prog) {
		*size = 0;
		return NULL;
	}
	*size = prog->payloadLen;
	prog->foundStart = false;
	return (const char *)prog->payload;
}

#if 0
static int findChannelPMTPID(unsigned char *data, int program)
{
	mDebug("searching PMT program id for program %d", program);
	int afe = (*data++ >> 4) & 3;
	if (afe == 0)
		return 0;
	if (afe > 1) {
		data += *data + 1;
	}
	if (afe & 0x1) {
		/* handle pointer_field */
		data += *data + 1;
		int sl = (((data[1] >> 0) & 0xf) << 8) + data[2];
		unsigned char *pmtid = data + 8;
		for (int i = 0; i < sl - 9; i += 4) {
			if (((pmtid[i] << 8) + pmtid[i + 1]) == program) {
				mDebug("found PMT program id for program %d: %d", program,
					   ((pmtid[i + 2] & 0x1f) << 8) + pmtid[i + 3]);
				return ((pmtid[i + 2] & 0x1f) << 8) + pmtid[i + 3];
			}
		}
	}

	return 0;
}
#endif
