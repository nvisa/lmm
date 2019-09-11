#ifndef METRICS_H
#define METRICS_H

#ifdef HAVE_METRICS_PROMETHEUS
#include "metricslib.h"
#else
class MetricsLib
{
public:
	virtual int start() { return 0; }
};

#endif

#ifdef HAVE_METRICS_PROMETHEUS
	#define addCounterMetric(_x) \
		public: \
			static inline void _x##Inc() { Metrics::instance()._x->Increment(); } \
			static inline void _x##Inc(double value) { Metrics::instance()._x->Increment(value); } \
		protected: \
			Counter *_x
#else
	#define addCounterMetric(_x) \
		public: \
			static inline void _x##Inc() {} \
			static inline void _x##Inc(double) {} \
		protected: \
			void *_x /* to prevent compiler/ide warning */
#endif

#ifdef HAVE_METRICS_PROMETHEUS
	#define addGaugeMetric(_x) \
		public: \
			static inline void _x##Inc() { Metrics::instance()._x->Increment(); } \
			static inline void _x##Inc(double value) { Metrics::instance()._x->Increment(value); } \
			static inline void _x##Dec() { Metrics::instance()._x->Decrement(); } \
			static inline void _x##Dec(double value) { Metrics::instance()._x->Decrement(value); } \
		protected: \
			Gauge *_x
#else
	#define addGaugeMetric(_x) \
		public: \
			static inline void _x##Inc() {} \
			static inline void _x##Inc(double) {} \
			static inline void _x##Dec() {} \
			static inline void _x##Dec(double) {} \
			void *_x /* to prevent compiler/ide warning */
#endif

class Metrics : public MetricsLib
{
public:
	static Metrics & instance();
	int start();

	addGaugeMetric(inVideoStreams);
	addGaugeMetric(outVideoStreams);

	/* rtp input specific */
	addCounterMetric(inRtpPacketsValid);
	addCounterMetric(inRtpPacketsHeaderErr);
	addCounterMetric(inRtpPacketsPayloadErr);
	addCounterMetric(inRtpPacketsRR);
	addCounterMetric(inRtpPacketsSR);
	addCounterMetric(inRtpBytesValid);
	addCounterMetric(inRtpBytesHeaderErr);
	addCounterMetric(inRtpBytesPayloadErr);
	addCounterMetric(inRtpBytesPayload);
	addCounterMetric(inRtpBytesPayloadH264);
	addCounterMetric(inRtpBytesPayloadJPEG);
	addCounterMetric(inRtpBytesPayloadMeta);
	addCounterMetric(inRtpBytesPayloadUnknown);

	/* rtp output specific */
	addCounterMetric(outRtpPackets);
	addCounterMetric(outRtpBytes);
	addCounterMetric(outRtpPacketsRR);
	addCounterMetric(outRtpPacketsSR);
	addCounterMetric(outRtpTimeout);
	addCounterMetric(outRtpDataBytes);
	addGaugeMetric(outRtpChannelCount);
	addGaugeMetric(outRtpMulticastChannelCount);
	addGaugeMetric(outRtpPlayingChannelCount);

	/* rtsp specific */
	addCounterMetric(inRtspRequest);
	addCounterMetric(inRtspErrorLastLine);
	addCounterMetric(inRtspHttpTunnelledGet);
	addCounterMetric(inRtspHttpErrorCode);
	addCounterMetric(inRtspUnknownCommand);
	addCounterMetric(inRtspOptionsCommand);
	addCounterMetric(inRtspDescribeCommand);
	addCounterMetric(inRtspSetupCommand);
	addCounterMetric(inRtspPlayCommand);
	addCounterMetric(inRtspTeardownCommand);
	addCounterMetric(inRtspGetParameterCommand);
	addCounterMetric(inRtspSetParameterCommand);
	addGaugeMetric(inRtspSession);
	addGaugeMetric(inRtspStreamCount);
	addGaugeMetric(inRtspSettedUpSession);
	addGaugeMetric(inRtspSettedUpTCPSession);
	addGaugeMetric(inRtspSettedUpUDPSession);
	addGaugeMetric(inRtspSettedUpMulticastSession);

	/* pipeline related */
	addGaugeMetric(pRawBuffers);
	addGaugeMetric(pRawBuffersData);

protected:
	Metrics();

	void addRtpMetrics();
	void addPipelineMetrics();

#ifdef HAVE_METRICS_PROMETHEUS
	Family<Gauge> *streams;
	Family<Counter> *rtpStats;
	Family<Gauge> *rtpInformation;
	Family<Gauge> *rtspInformation;
	Family<Counter> *rtspStats;
	Family<Gauge> *pipelineInformation;
	Family<Counter> *pipelineStats;
#endif

#if 0
	Family<Counter> *transfers;
	Family<Counter> *bytes;
	Counter *inVideoBuffers;
	Counter *inVideoBytes;
	Counter *outVideoBuffers;
	Counter *outVideoBytes;
	Counter *readVideoBuffers;
	Counter *readVideoBytes;
	Counter *writtenVideoBuffers;
	Counter *writtenVideoBytes;
#endif

};

#endif // METRICS_H
