#include "metrics.h"

Metrics &Metrics::instance()
{
	static Metrics m;
	return m;
}

int Metrics::start()
{
#ifdef HAVE_METRICS_PROMETHEUS
	streams = &addGauge("total_stream_count", "Number of streams present in the system");
	inVideoStreams = &addLabel(*streams, {{"direction", "in"}, {"media", "video"}});
	outVideoStreams = &addLabel(*streams, {{"direction", "out"}, {"media", "video"}});
	addRtpMetrics();
	addPipelineMetrics();
#endif

#if 0
	transfers = &addCounter("transceived_buffers", "Number of buffers transceived");
	inVideoBuffers = &addLabel(*transfers, {{"direction", "in"},
											{"media", "video"},
											{"layer", "network"}
							   });
	outVideoBuffers = &addLabel(*transfers, {{"direction", "out"},
											 {"media", "video"},
											 {"layer", "network"}
								});
	readVideoBuffers = &addLabel(*transfers, {{"direction", "in"},
											{"media", "video"},
											{"layer", "scsi"}
							   });
	writtenVideoBuffers = &addLabel(*transfers, {{"direction", "out"},
											 {"media", "video"},
											 {"layer", "scsi"}
								});

	bytes = &addCounter("transceived_bytes", "Bytes of data transceived");
	inVideoBytes = &addLabel(*bytes, {{"direction", "in"},
											{"media", "video"},
											{"layer", "network"}
							   });
	outVideoBytes = &addLabel(*bytes, {{"direction", "out"},
											{"media", "video"},
											{"layer", "network"}
							   });
	readVideoBytes = &addLabel(*bytes, {{"direction", "in"},
											{"media", "video"},
											{"layer", "scsi"}
							   });
	writtenVideoBytes = &addLabel(*bytes, {{"direction", "out"},
											{"media", "video"},
											{"layer", "scsi"}
							   });
#endif

	return MetricsLib::start();
}

Metrics::Metrics()
	: MetricsLib()
{

}

void Metrics::addRtpMetrics()
{
#ifdef HAVE_METRICS_PROMETHEUS
	/* families */
	rtpStats = &addCounter("rtp_stats", "RTP send and receive statistics");
	rtpInformation = &addGauge("rtp_info", "RTP information");
	rtspInformation = &addGauge("rtsp_info", "RTSP information");
	rtspStats = &addCounter("rtsp_stats", "RTSP statistics");

	/* input labels */
	inRtpPacketsValid = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "valid_packets"}});
	inRtpPacketsHeaderErr = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "header_error_packets"}});
	inRtpPacketsPayloadErr = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "payload_error_packets"}});
	inRtpPacketsRR = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "rtcp_rr"}});
	inRtpPacketsSR = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "rtcp_sr"}});
	inRtpBytesValid = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "valid_bytes"}});
	inRtpBytesHeaderErr = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "header_error_bytes"}});
	inRtpBytesPayloadErr = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "payload_error_bytes"}});
	inRtpBytesPayload = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "payload_bytes"}});
	inRtpBytesPayloadH264 = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "payload_bytes_h264"}});
	inRtpBytesPayloadJPEG = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "payload_bytes_jpeg"}});
	inRtpBytesPayloadMeta = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "payload_bytes_meta"}});
	inRtpBytesPayloadUnknown = &addLabel(*rtpStats, {{"direction", "in"}, {"type", "payload_bytes_uknown"}});

	/* output labels */
	outRtpPackets = &addLabel(*rtpStats, {{"direction", "out"}, {"type", "all_packets"}});
	outRtpPacketsRR = &addLabel(*rtpStats, {{"direction", "out"}, {"type", "rtcp_rr"}});
	outRtpPacketsSR = &addLabel(*rtpStats, {{"direction", "out"}, {"type", "rtcp_sr"}});
	outRtpTimeout = &addLabel(*rtpStats, {{"direction", "out"}, {"type", "timeouts"}});
	outRtpChannelCount = &addLabel(*rtpInformation, {{"direction", "out"}, {"type", "channels"}});
	outRtpMulticastChannelCount = &addLabel(*rtpInformation, {{"direction", "out"}, {"type", "multicast_channels"}});
	outRtpPlayingChannelCount = &addLabel(*rtpInformation, {{"direction", "out"}, {"type", "active_channels"}});
	outRtpBytes = &addLabel(*rtpStats, {{"direction", "out"}, {"type", "bytes"}});
	outRtpDataBytes = &addLabel(*rtpStats, {{"direction", "out"}, {"type", "data_bytes"}});

	/* rtsp metrics */
	inRtspRequest = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "all_requests"}});
	inRtspErrorLastLine = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "error"}, {"code", "line_ending"}});
	inRtspHttpErrorCode = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "error"}, {"code", "http"}});
	inRtspHttpTunnelledGet = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "http_tunnel"}});
	inRtspUnknownCommand = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "unknown_commands"}});
	inRtspOptionsCommand = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "options_commands"}});
	inRtspDescribeCommand = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "describe_commands"}});
	inRtspSetupCommand = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "setup_commands"}});
	inRtspPlayCommand = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "play_commands"}});
	inRtspTeardownCommand = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "teardown_commands"}});
	inRtspGetParameterCommand = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "get_parameter_commands"}});
	inRtspSetParameterCommand = &addLabel(*rtspStats, {{"direction", "in"}, {"type", "set_parameter_commands"}});
	inRtspSession = &addLabel(*rtspInformation, {{"direction", "in"}, {"type", "sessions"}, {"cat", "all"}});
	inRtspSettedUpTCPSession = &addLabel(*rtspInformation, {{"direction", "in"}, {"type", "sessions"}, {"cat", "tcp"}});
	inRtspSettedUpUDPSession = &addLabel(*rtspInformation, {{"direction", "in"}, {"type", "sessions"}, {"cat", "udp"}});
	inRtspSettedUpMulticastSession = &addLabel(*rtspInformation, {{"direction", "in"}, {"type", "sessions"}, {"cat", "multicast"}});
	inRtspStreamCount = &addLabel(*rtspInformation, {{"direction", "in"}, {"type", "streams"}});
#endif
}

void Metrics::addPipelineMetrics()
{
	pipelineInformation = &addGauge("pipeline_information", "Detailed pipeline information");
	pipelineStats = &addCounter("pipeline_stats", "Detailed pipeline statistics");

	pRawBuffers = &addLabel(*pipelineInformation, {{"buffers", "count"}});
}

