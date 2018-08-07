#include "genericstreamer.h"
#include "mjpegserver.h"
#include "audiosource.h"
#include "metadatagenerator.h"
#include "seiinserter.h"

#include <lmm/alsa/alsainput.h>
#include <lmm/alsa/alsaoutput.h>

#include <ecl/debug.h>
#include <ecl/uuid/uuid.h>
#include <ecl/drivers/systeminfo.h>
#include <ecl/drivers/hardwareoperations.h>
#include <ecl/settings/applicationsettings.h>

#include <lmm/h264parser.h>
#include <lmm/textoverlay.h>
#include <lmm/bufferqueue.h>
#include <lmm/tools/cpuload.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/dmai/h264encoder.h>
#include <lmm/dmai/jpegencoder.h>
#include <lmm/rtp/rtptransmitter.h>
#include <lmm/dm365/dm365dmacopy.h>
#include <lmm/rtsp/basertspserver.h>
#include <lmm/dm365/dm365camerainput.h>
#include <lmm/pipeline/functionpipeelement.h>
#include <lmm/interfaces/streamcontrolelementinterface.h>

#include <QBuffer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileSystemWatcher>

#include <errno.h>

#define getss(nodedef) gets(s, pre, nodedef)

static QString getMacAddress(const QString &iface)
{
	QFile f(QString("/sys/class/net/%1/address").arg(iface));
	if (!f.open(QIODevice::ReadOnly))
		return "ff:ff:ff:ff:ff:ff";
	return QString::fromUtf8(f.readAll()).trimmed();
}

static QString createUuidV3(const QString &mac)
{
	char *vp = NULL;
	size_t n;
	uuid_t *uuid;
	uuid_t *uuid_ns;
	uuid_create(&uuid);
	uuid_create(&uuid_ns);
	uuid_load(uuid_ns, "ns:OID");
	uuid_make(uuid, UUID_MAKE_V3, uuid_ns, qPrintable(mac));
	uuid_destroy(uuid_ns);
	uuid_export(uuid, UUID_FMT_STR, &vp, &n);
	QString s = QString::fromUtf8(vp);
	free(vp);
	return s;
}

static inline void serH264IntData(QDataStream &out, qint32 data)
{
	if (data < 5)
		data += 5;
	out << data;
	out << (qint32)0xffffffff;
}

static QHash<int, int> getInterrupts()
{
	QHash<int, int> count;
	QFile f("/proc/interrupts");
	f.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
	QStringList lines = QString::fromUtf8(f.readAll()).split("\n");
	foreach (const QString &line, lines) {
		QStringList flds = line.split(" ", QString::SkipEmptyParts);
		if (flds.size() < 4)
			continue;
		count[flds[0].remove(":").toInt()] = flds[1].toInt();
	}
	return count;
}

float getVideoFps(int pipeline)
{
	ApplicationSettings *s = ApplicationSettings::instance();
	return s->get(QString("config.pipeline.%1.frame_skip.out_fps").arg(pipeline)).toFloat();
}

static QVariant gets(ApplicationSettings *s, const QString &prefix, const QString &nodedef)
{
	return s->get(QString("%1.%2").arg(prefix).arg(nodedef));
}

GenericStreamer::GenericStreamer(QObject *parent) :
	BaseStreamer(parent)
{
	ApplicationSettings *s = ApplicationSettings::instance();

	ApplicationSettings *vks = ApplicationSettings::create("/etc/encsoft/vksystem.json", QIODevice::ReadOnly);
	int gttl = vks->get("network_settings.general_ttl").toInt();
	if (gttl) {
		QFile f("/proc/sys/net/ipv4/ip_default_ttl");
		f.open(QIODevice::WriteOnly | QIODevice::Unbuffered);
		f.write(QString("%1\n").arg(gttl).toUtf8());
		f.close();
	}
	int rtpMtu = vks->getm("network_settings.rtp_mtu").toInt();
	delete vks;

	mainTextOverlay = NULL;
	pbus = new LmmProcessBus(this, this);
	pbus->join();
	lastIrqk = 0;
	lastIrqkSource = 0;
	wdogimpl = s->get("config.watchdog_implementation").toInt();
	customSei.inited = false;

	uuid = createUuidV3(getMacAddress("eth0"));

	initOnvifBindings();
	reloadEarlyOnvifBindings();

	/* rtsp server */
	QString rtspConfig = s->get("video_encoding.rtsp.stream_config").toString();
	int rtspPort = s->get("video_encoding.rtsp.server_port").toInt();
	if (!rtspPort)
		rtspPort = 554;
	mDebug("using '%s' RTSP config", qPrintable(rtspConfig));
	rtsp = new BaseRtspServer(this, rtspPort);
	rtsp->setEnabled(false);
	rtsp->setRtspAuthentication((BaseRtspServer::Auth)s->get("video_encoding.rtsp.auth").toInt());
	rtsp->setRtspAuthenticationCredentials(s->get("video_encoding.rtsp.username").toString(),
										   s->get("video_encoding.rtsp.password").toString());
	rtspCredHashData += s->get("video_encoding.rtsp.username").toString();
	rtspCredHashData += s->get("video_encoding.rtsp.password").toString();
	int tv = s->get("video_encoding.rtsp.timeout_value").toInt();
	if (tv)
		rtsp->setRtspTimeoutValue(tv);

	/* camera input settings */
	int cameraInputType = s->get("camera_device.input_type").toInt();
	int vbp = s->get("camera_device.input_vbp").toInt();
	int hbp = s->get("camera_device.input_hbp").toInt();
	int w0 = s->get("camera_device.ch.0.width").toInt();
	int h0 = s->get("camera_device.ch.0.height").toInt();
	int w1 = s->get("camera_device.ch.1.width").toInt();
	int h1 = s->get("camera_device.ch.1.height").toInt();
	int wa[] = {w0, w1};
	int ha[] = {h0, h1};
	camIn = new DM365CameraInput();
	if (cameraInputType) /* we assume no one needs older composite input type for the sake of sane defaults */
		camIn->setInputType((DM365CameraInput::cameraInput)cameraInputType);
	camIn->setNonStdOffsets(vbp, hbp);
	camIn->setSize(0, QSize(w0, h0));
	camIn->setSize(1, QSize(w1, h1));

	QHash<QString, BaseLmmElement *> allElements;
	int err = 0;
	int cnt = s->getArraySize("config.pipeline");
	for (int i = 0; i < cnt; i++) {
		int pipelineMaxTimeout = 0;
		QString p = QString("config.pipeline.%1").arg(i);

		bool fsEnabled = gets(s, p, "frame_skip.enabled").toBool();
		float fsIn = gets(s, p, "frame_skip.in_fps").toFloat();
		float fsOut = gets(s, p, "frame_skip.out_fps").toFloat();
		int fsTarget = gets(s, p, "frame_skip.target_element").toInt();
		int fsQueue = gets(s, p, "frame_skip.target_queue").toInt();

		if (!s->get(QString("%1.enabled").arg(p)).toBool())
			continue;

		mDebug("setting-up pipeline %d", i);

		QList<QPair<int, StreamControlElementInterface *> > streamControls;
		QList<BaseLmmElement *> elements;
		int ecnt = s->getArraySize(QString("%1.elements").arg(p));
		for (int j = 0; j < ecnt; j++) {
			QString pre = QString("%1.elements.%2").arg(p).arg(j);
			QString type = gets(s, pre, "type").toString();
			QString name = gets(s, pre, "object_name").toString();
			bool enabled = gets(s, pre, "enabled").toBool();
			if (!enabled)
				continue;

			BaseLmmElement *el = NULL;

			if (allElements.contains(name)) {
				el = allElements[name];
			} else if (type == "TextOverlay") {
				mainTextOverlay = createTextOverlay(name, s);
				el = mainTextOverlay;
			} else if (type == "H264Encoder") {
				int ch = getss("channel").toInt();
				el = createH264Encoder(name, s, ch, wa[ch], ha[ch]);
				pipelineMaxTimeout = 5000;
			} else if (type == "JpegEncoder") {
				int ch = getss("channel").toInt();
				el = createJpegEncoder(name, s, ch, wa[ch], ha[ch]);
				el->start();
			} else if (type == "MjpegElement") {
				MjpegElement *mjpeg = new MjpegElement(getss("port").toInt(), this);
				el = mjpeg;

				if (getss("stream_control.enabled").toBool()) {
					int elCount = s->getArraySize(QString("%1.stream_control.targets").arg(pre));
					for (int k = 0; k < elCount; k++) {
						pre = QString("%1.elements.%2.stream_control.targets.%3").arg(p).arg(j).arg(k);
						streamControls << QPair<int, StreamControlElementInterface *>
								(getss("control_element_index").toInt(), mjpeg);
					}
				}
			} else if (type == "SeiInserter") {
				SeiInserter *sel = new SeiInserter(this);
				pre = QString("%1.elements.%2.alarm").arg(p).arg(j);
				int seierr = sel->setAlarmInformation(getss("template").toString(),
										 getss("algorithm").toInt(),
										 getss("gpio").toInt(),
										 getss("io_alarm_level").toInt(),
										 getss("minimum_alarm_duration").toInt(),
										 gets(s,"video_encoding.ch.0", "motion_sensitivity").toInt()
										 );
				mDebug("SEI algorithm adjusted with err '%d'", seierr);
				el = sel;
			} else if (type == "DM365DmaCopy") {
				DM365DmaCopy *dma = new DM365DmaCopy(this, getss("output_count").toInt());
				dma->setBufferCount(getss("buffer_count").toInt());
				dma->setAllocateSize(getss("alloc_size").toInt());
				el = dma;
			} else if (type == "BufferQueue") {
				BufferQueue *bq = new BufferQueue(this);
				bq->setQueueSize(getss("queue_size").toInt());
				el = bq;
			} else if (type == "RtpTransmitter") {
				QString codec = getss("codec").toString();
				RtpTransmitter *rtp = NULL;
				if (codec.isEmpty() || codec == "h264")
					rtp = new RtpTransmitter(this);
				else if (codec == "g711")
					rtp = new RtpTransmitter(this, Lmm::CODEC_PCM_ALAW);
				else if (codec == "pcm_L16")
					rtp = new RtpTransmitter(this, Lmm::CODEC_PCM_L16);
				else if (codec == "meta_bilkon")
					rtp = new RtpTransmitter(this, Lmm::CODEC_META_BILKON);
				else if (codec == "jpeg")
					rtp = new RtpTransmitter(this, Lmm::CODEC_JPEG);
				rtp->setMulticastTTL(getss("multicast_ttl").toInt());
				if (gttl)
					rtp->setMulticastTTL(gttl);
				if (rtpMtu)
					rtp->setMaximumPayloadSize(rtpMtu);
				else
					rtp->setMaximumPayloadSize(getss("rtp_max_payload_size").toInt());
				rtp->setRtcp(!getss("disable_rtcp").toBool());
				rtp->setTrafficShaping(getss("traffic_shaping").toBool(),
									   getss("traffic_shaping_average").toInt(),
									   getss("traffic_shaping_burst").toInt(),
									   getss("traffic_shaping_duration").toInt());
				if (getss("object_name") == "RtpHigh")
					rtp->setH264SEIInsertion(s->get("video_encoding.ch.0.sei_enabled").toBool());
				int tv = getss("rtcp_timeout").toInt();
				if (tv)
					rtp->setRtcpTimeoutValue(tv);
				el = rtp;

				int streamCount = s->getArraySize(QString("%1.rtsp").arg(pre));
				for (int k = 0; k < streamCount; k++) {
					pre = QString("%1.elements.%2.rtsp.%3").arg(p).arg(j).arg(k);
					QString streamName = getss("stream").toString();
					if (!rtsp->hasStream(streamName))
						rtsp->addStream(streamName, getss("multicast").toBool(), rtp,
									getss("port").toInt(),
									getss("multicast_address").toString());
					rtsp->addMedia2Stream(getss("media").toString(),
										  streamName,
										  getss("multicast").toBool(),
										  rtp,
										  getss("port").toInt(),
										  getss("multicast_address").toString());
					if (!getss("multicast_address_base").toString().isEmpty())
						rtsp->setMulticastAddressBase(streamName, getss("media").toString(), getss("multicast_address_base").toString());

					QString media = getss("media").toString();
					bool tse = getss("traffic_shaping").toBool();
					if (tse) {
						rtsp->addStreamParameter(streamName, media, "TrafficShapingEnabled", true);
						rtsp->addStreamParameter(streamName, media, "TrafficShapingAverage", getss("traffic_shaping_average").toInt());
						rtsp->addStreamParameter(streamName, media, "TrafficShapingBurst", getss("traffic_shaping_burst").toInt());
						rtsp->addStreamParameter(streamName, media, "TrafficShapingDuration", getss("traffic_shaping_duration").toInt());
					}
					int unicastCount = getss("max_unicast_count").toInt();
					if (unicastCount)
						rtsp->addStreamParameter(streamName, media, "MaxUnicastStreamCount", unicastCount);
				}
				pre = QString("%1.elements.%2").arg(p).arg(j);

				if (getss("stream_control.enabled").toBool()) {
					int elCount = s->getArraySize(QString("%1.stream_control.targets").arg(pre));
					for (int k = 0; k < elCount; k++) {
						pre = QString("%1.elements.%2.stream_control.targets.%3").arg(p).arg(j).arg(k);
						streamControls << QPair<int, StreamControlElementInterface *>
								(getss("control_element_index").toInt(), rtp);
					}
				}
			} else if (type == "DM365CameraInput") {
				el = camIn;
			} else if (type == "AlsaInput") {
				Lmm::CodecType acodec = Lmm::CODEC_PCM_ALAW;
				QString codecName = getss("codec_name").toString();
				if (codecName == "g711")
					acodec = Lmm::CODEC_PCM_ALAW;
				else if (codecName == "L16")
					acodec = Lmm::CODEC_PCM_L16;
				AlsaInput *alsaIn = new AlsaInput(acodec, this);
				alsaIn->setParameter("audioRate", getss("rate").toInt());
				alsaIn->setParameter("sampleCount", getss("buffer_size").toInt());
				alsaIn->setParameter("channelCount", getss("channels").toInt());
				el = alsaIn;
			} else if (type == "AlsaOutput") {
				AlsaOutput *alsaOut = new AlsaOutput(this);
				alsaOut->setParameter("audioRate", getss("rate").toInt());
				alsaOut->setParameter("channelCount", getss("channels").toInt());
				el = alsaOut;
			} else if (type == "AudioSource") {
				AudioSource *src = new AudioSource(this);
				el = src;
			} else if (type == "MetadataGenerator") {
				MetadataGenerator *mgen = new MetadataGenerator(this);
				metaGenerators << mgen;
				el = mgen;
			} else if (type == "CustomSeiFunction1") {

				el = newFunctionPipe(GenericStreamer, this, GenericStreamer::generateCustomSEI);
			} else if (type == "CustomSeiFunction2") {
				el = newFunctionPipe(GenericStreamer, this, GenericStreamer::generateCustomSEI2);
			}

			if (!el) {
				mDebug("no suitable element for node %d (%s)", j, qPrintable(type));
				err = -ENOENT;
				break;
			}
			if (!name.isEmpty())
				el->setObjectName(name);
			mDebug("adding element %s with type %s to pipeline %d", qPrintable(name), qPrintable(type), i);
			elements << el;
			allElements.insert(name, el);
		}

		BaseLmmPipeline *pl = addPipeline();
		for (int j = 0; j < elements.size(); j++)
			pl->append(elements[j]);
		pl->setMaxTimeout(pipelineMaxTimeout);
		pl->end();
		for (int i = 0; i < streamControls.size(); i++) {
			const QPair<int, StreamControlElementInterface *> &p = streamControls[i];
			streamControl[pl] << p.first;
			streamControlElement[pl] << p.second;
		}

		if (fsEnabled)
			pl->getPipe(fsTarget)->getOutputQueue(fsQueue)->setRateReduction(fsIn, fsOut);

		postInitPipeline(pl);
	}

	if (s->get("camera_device.invert_clock").toBool())
		HardwareOperations::writeRegister(0x1c40044, 0x1c);
	else
		HardwareOperations::writeRegister(0x1c40044, 0x18);

	lockCheckTimer.start();

	noRtspContinueSupport = s->get("video_encoding.rtsp.no_continue_support").toBool();
	if (noRtspContinueSupport == false) {
		err = rtsp->loadSessions("/tmp/rtsp.sessions");
		mDebug("RTSP session load from cache %d", err);
	}

	reloadLateOnvifBindings();
}

QList<RawBuffer> GenericStreamer::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
{
	return QList<RawBuffer>();
#if 0
	if (codec == Lmm::CODEC_H264) {
		QList<RawBuffer> bufs;
		if (ts)
			bufs = h264Queue->findBuffers(ts, frameCount);
		else
			bufs = h264Queue->getLast(frameCount);
		return bufs;
	}

	/* raw or jpeg snapshot */
	JpegEncoder *enc = encJpegHigh;
	BufferQueue *queue = rawQueue;
	if (ch == 1) {
		queue = rawQueue2;
		enc = encJpegLow;
	}
	QList<RawBuffer> bufs;
	if (ts)
		bufs = queue->findBuffers(ts, frameCount);
	else
		bufs = queue->getLast(frameCount);
	if (codec == Lmm::CODEC_RAW)
		return bufs;
	QList<RawBuffer> bufs2;
	/* we use while loop so that encoded buffers can be released */
	while (bufs.size()) {
		enc->addBuffer(0, bufs.takeFirst());
		enc->processBlocking();
		bufs2 << enc->nextBufferBlocking(0);
	}
	return bufs2;
#endif
}

void GenericStreamer::timeout()
{
	if (!checkPipelineWdts) {
		/* we are not fully ready yet */
		bool allRunning = true;
		pipelineLock.lock();
		for (int i = 0; i < getPipelineCount(); i++) {
			BaseLmmPipeline *pl = getPipeline(i);
			if (!pl->getStats().outCount)
				allRunning = false;
		}
		pipelineLock.unlock();
		if (allRunning) {
			mDebug("All pipelines ready, starting");
			checkPipelineWdts = true;
			rtsp->setEnabled(true);
		}
	}
	PipelineManager::timeout();

	if (customSei.inited) {
		customSei.lock.lock();
		customSei.rtspSessionCount = rtsp->getSessions().size();
		if (customSei.t.elapsed() > 5000) {
			customSei.t.restart();
			customSei.cpuload = CpuLoad::getAverageCpuLoad();
			customSei.freemem = SystemInfo::getFreeMemory();
			customSei.uptime = SystemInfo::getUptime();

			customSei.out.device()->seek(customSei.cpuLoadPos);
			serH264IntData(customSei.out, customSei.cpuload);
			serH264IntData(customSei.out, customSei.freemem);
			serH264IntData(customSei.out, customSei.uptime);
		}
		customSei.lock.unlock();
	}
}

void GenericStreamer::onvifChanged(const QString &filename)
{
	ffDebug() << filename;
	if (reloadEarlyOnvifBindings())
		QCoreApplication::instance()->quit();
	if (reloadLateOnvifBindings())
		QCoreApplication::instance()->quit();
}

void GenericStreamer::postInitPipeline(BaseLmmPipeline *p)
{
	for (int i = 0; i < p->getPipeCount(); i++) {
		BaseLmmElement *el = p->getPipe(i);
		SeiInserter *sel = qobject_cast<SeiInserter *>(el);
		if (sel && i + 1 < p->getPipeCount()) {
			BaseLmmElement *nel = p->getPipe(i - 2);
			H264Encoder *eel = qobject_cast<H264Encoder *>(nel);
			if (eel)
				sel->setMotionDetectionProvider(eel);
		}
		MjpegElement *mjpeg = qobject_cast<MjpegElement *>(el);
		if (mjpeg) {
			mDebug("Rate limit MJPEG ***************************");
			mjpeg->getInputQueue(0)->setRateLimitBufferCount(3, true);
		}
	}

	if (ApplicationSettings::instance()->get("video_encoding.ch.0.sei_enabled").toBool())
		initCustomSEI();
}

int GenericStreamer::pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf)
{
	for (int i = 0; i < streamControl[p].size(); i++) {
		int sci = streamControl[p][i];
		if (sci != -1) {
			StreamControlElementInterface *sce = streamControlElement[p][i];
			BaseLmmElement *el = p->getPipe(sci);
			bool active = sce->isActive();
			if (active && el->isPassThru()) {
				mDebug("enabling pipeline control point: %d", sci);
				for (int i = sci; i < p->getPipeCount(); i++)
					p->getPipe(i)->setPassThru(false);
			} else if (!active && !el->isPassThru()) {
				mDebug("disabling pipeline control point: %d", sci);
				for (int i = p->getPipeCount() - 1; i >= sci; i--)
					p->getPipe(i)->setPassThru(true);
			}
			break;
		}
	}

	if (noRtspContinueSupport == false)
		rtsp->saveSessions("/tmp/rtsp.sessions");

	return 0;
}

#define checkSessionPar(member_name) \
	if (info == #member_name) \
		return ses->member_name
#define printSessionPar(member_name) lines << QString("\t%1: %2").arg(#member_name).arg(ses->member_name)
QVariant GenericStreamer::getRtspStats(const QString &fld)
{
	if (fld == "rtsp.stats") {
		/* dumping */
		QStringList lines;
		const QStringList list = rtsp->getSessions();
		foreach (QString s, list) {
			const BaseRtspSession *ses = rtsp->getSession(s);
			lines << "---------------------------------------";
			Q_UNUSED(ses);
			printSessionPar(state);
			printSessionPar(transportString);
			printSessionPar(multicast);
			printSessionPar(controlUrl);
			printSessionPar(streamName);
			printSessionPar(mediaName);
			printSessionPar(sourceDataPort);
			printSessionPar(sourceControlPort);
			printSessionPar(dataPort);
			printSessionPar(controlPort);
			printSessionPar(peerIp);
			printSessionPar(streamIp);
			printSessionPar(clientCount);
			printSessionPar(ssrc);
			printSessionPar(ttl);
			printSessionPar(rtptime);
			printSessionPar(seq);
			printSessionPar(rtspTimeoutEnabled);
			printSessionPar(rtspTimeoutValue);
		}

		return lines.join("\n");
	}
	if (fld == "rtsp.stats.session_count")
		return rtsp->getSessions().size();
	if (fld == "rtsp.stats.session_list")
		return rtsp->getSessions();
	if (fld.startsWith("rtsp.stats.session.")) {
		QStringList flds = fld.split(".");
		if (flds.size() < 5)
			return -EINVAL;
		QString session = flds[3];
		QString info = flds[4];
		const BaseRtspSession *ses = rtsp->getSession(session);
		Q_UNUSED(info);
		Q_UNUSED(ses);
		checkSessionPar(state);
		checkSessionPar(transportString);
		checkSessionPar(multicast);
		checkSessionPar(controlUrl);
		checkSessionPar(streamName);
		checkSessionPar(mediaName);
		checkSessionPar(sourceDataPort);
		checkSessionPar(sourceControlPort);
		checkSessionPar(dataPort);
		checkSessionPar(controlPort);
		checkSessionPar(peerIp);
		checkSessionPar(streamIp);
		checkSessionPar(clientCount);
		checkSessionPar(ssrc);
		checkSessionPar(ttl);
		checkSessionPar(rtptime);
		checkSessionPar(seq);
		checkSessionPar(rtspTimeoutEnabled);
		checkSessionPar(rtspTimeoutValue);
		if (info == "siblings") {
			QStringList list;
			foreach (BaseRtspSession *s, ses->siblings)
				list << s->sessionId;
			return list;
		}
	}
	return -ENOENT;
}

int GenericStreamer::getWdogKey()
{
	if (!wdogimpl)
		return 0;

	if (lockCheckTimer.elapsed() < 30000)
		return 0;
	lockCheckTimer.restart();

	int scnt = rtsp->getSessions("stream1").size()
			+ rtsp->getSessions("stream1m").size()
			+ rtsp->getSessions("stream2").size()
			+ rtsp->getSessions("stream2m").size()
			+ rtsp->getSessions("stream101").size()
			+ rtsp->getSessions("stream101m").size();
	int err = -1;
	const QHash<int, int> cnt = getInterrupts();
	int irqk = 0;
	if (scnt) {
		/* we have streaming, using irqk */
		if (cnt.contains(9))
			irqk += cnt[9];
		if (cnt.contains(10))
			irqk += cnt[10];
		if (lastIrqkSource == 0)
			lastIrqk = irqk - 1;
		lastIrqkSource = 9;
	} else {
		/* we don't have streaming, we use vpfe interrupt */
		if (cnt.contains(0))
			irqk += cnt[0];
		if (lastIrqkSource)
			lastIrqk = irqk - 1;
		lastIrqkSource = 0;
	}

	if (irqk > lastIrqk)
		err = 0;
	lastIrqk = irqk;

	if ((wdogimpl & 0x2) && lastIrqkSource == 9 && err) {
		/* we should quit w/o waiting watchdog, let's schedule a restart */
		QTimer::singleShot(2000, QCoreApplication::instance(), SLOT(quit()));
		/* leave some time for wdog reset */
		err = 0;
	}

	return err;
}

int GenericStreamer::generateCustomSEI(const RawBuffer &buf)
{
	if (!customSei.inited)
		return 0;
	int nal = buf.constPars()->h264NalType;
	if (nal == SimpleH264Parser::NAL_SLICE || nal == SimpleH264Parser::NAL_SLICE_IDR) {
		customSei.lock.lock();

		customSei.out.device()->seek(customSei.encodeCountPos);
		serH264IntData(customSei.out, buf.constPars()->streamBufferNo);
		serH264IntData(customSei.out, customSei.rtspSessionCount);
		if (customSei.plStatsPos) {
			customSei.out.device()->seek(customSei.plStatsPos);
			BaseLmmPipeline *pl = getPipeline(0);
			serH264IntData(customSei.out, pl->getPipeCount() + 5);
			for (int i = 0; i < pl->getPipeCount(); i++) {
				int bytes = pl->getPipe(i)->getTotalMemoryUsage();
				serH264IntData(customSei.out, i);
				serH264IntData(customSei.out, bytes + 5);
			}
		}

		RawBuffer *buf2 = (RawBuffer *)&buf;
		buf2->pars()->metaData = QByteArray(customSei.serdata.constData(), customSei.serdata.size());

		customSei.lock.unlock();

		/* write frame hash */
		char *hashData = (char *)buf2->constPars()->metaData.data() + customSei.frameHashPos;
		QByteArray hash = calculateFrameHash(buf);
		memcpy(hashData, hash.constData(), hash.size());
	}

	return 0;
}

int GenericStreamer::generateCustomSEI2(const RawBuffer &buf)
{
	RawBuffer *buf2 = (RawBuffer *)&buf;
	buf2->pars()->metaData = calculateFrameHash(buf);
	//ffDebug() << buf2->pars()->metaData.size();
	return 0;
}

QByteArray GenericStreamer::calculateFrameHash(const RawBuffer &buf)
{
	int nalType = ((RawBuffer *)&buf)->pars()->h264NalType;
	if (!(nalType == 1 || nalType == 5))	//1: P-frame, 5: I-frame
		return 0;

	QByteArray hash;
	const QByteArray &ba = QByteArray::fromRawData((const char *)buf.constData(), buf.size()).mid(5, buf.size() - 5);
	QCryptographicHash hashb(QCryptographicHash::Md5);

	int hlen = 0;
	int blen = ba.size() / 4;
	if (blen > 8192) {
		hlen = 8192;
		for (int i = 0; i < 4; i++) {
			hashb.addData(ba.mid(i * blen, hlen));
		}
	} else
		hashb.addData(ba);
	hashb.addData("Ekinoks_AVIYS_Aselsan_2018");
	if (rtsp->getRtspAuthentication() && rtspCredHashData.size())
		hashb.addData(rtspCredHashData.toUtf8());
	hash = hashb.result();
	return hash;
}

void GenericStreamer::initCustomSEI()
{
	mDebug("init custom SEI on encode channel 0");
	customSei.inited = true;
	customSei.t.start();
	customSei.cpuload = 0;
	customSei.freemem = 0;
	customSei.uptime = 0;
	customSei.pid = getpid();
	customSei.rtspSessionCount = 0;

	QBuffer *seibuf = new QBuffer(&customSei.serdata, this);
	seibuf->open(QIODevice::WriteOnly);
	customSei.out.setDevice(seibuf);
	customSei.out.setByteOrder(QDataStream::LittleEndian);
	customSei.out.setFloatingPointPrecision(QDataStream::SinglePrecision);

	customSei.out << (qint32)0x78984578;
	customSei.out << (qint32)0x11220105; //version

	customSei.cpuLoadPos = customSei.out.device()->pos();
	serH264IntData(customSei.out, customSei.cpuload);
	serH264IntData(customSei.out, customSei.freemem);
	serH264IntData(customSei.out, customSei.uptime);
	serH264IntData(customSei.out, customSei.pid);
	customSei.encodeCountPos = customSei.out.device()->pos();
	serH264IntData(customSei.out, (qint32)0);
	serH264IntData(customSei.out, customSei.rtspSessionCount + 5);
	customSei.plStatsPos = 0;
#if 1
	BaseLmmPipeline *pl = getPipeline(0);
	if (pl) {
		customSei.plStatsPos = customSei.out.device()->pos();
		serH264IntData(customSei.out, pl->getPipeCount() + 5);
		for (int i = 0; i < pl->getPipeCount(); i++) {
			int bytes = pl->getPipe(i)->getTotalMemoryUsage();
			serH264IntData(customSei.out, i);
			serH264IntData(customSei.out, bytes + 5);
		}
	} else
#endif
		serH264IntData(customSei.out, 5);
	/* write placeholder for frame hash */
	customSei.frameHashPos = customSei.out.device()->pos() + 4; //remember first there is length field(32-bit)
	QByteArray hash = QCryptographicHash::hash(QByteArray("0", 1), QCryptographicHash::Md5);
	customSei.out.writeBytes(hash.constData(), hash.size());
}

bool GenericStreamer::reloadEarlyOnvifBindings()
{
	bool needRestart = false;
	QFile f("/etc/encsoft/dbfolder/MediaCommon.json");
	f.open(QIODevice::ReadOnly);
	const QJsonObject &media = QJsonDocument::fromJson(f.readAll()).object();
	f.close();
	if (media.isEmpty())
		return needRestart;
	ApplicationSettings *s = ApplicationSettings::instance();
	QJsonArray encoders = media["VideoEncoders"].toObject()["Configurations"].toArray();
	for (int i = 0; i < encoders.size(); i++) {
		QJsonObject enc = encoders[i].toObject();
		QString token = enc["_attrs"].toArray().first().toObject()["value"].toString();
		QJsonObject mcast = enc["Multicast"].toObject();
		int ch = 0;
		if (token == "VideoEncoder0")
			ch = 0;
		else if (token == "VideoEncoder2")
			ch = 1;
		else
			continue;
		int _br = s->get(QString("video_encoding.ch.%1.bitrate").arg(ch)).toInt();
		int br = enc["RateControl"].toObject()["BitrateLimit"].toInt() * 1000;
		int gov = enc["H264"].toObject()["GovLength"].toInt();
		int _gov = s->get(QString("video_encoding.ch.%1.intraframe_interval").arg(ch)).toInt();
		if ((br != _br) || (gov != _gov))
			needRestart = true;
		s->set(QString("video_encoding.ch.%1.bitrate").arg(ch), br);
		s->set(QString("video_encoding.ch.%1.intraframe_interval").arg(ch), gov);
	}
	return needRestart;
}

bool GenericStreamer::reloadLateOnvifBindings()
{
	return false;
	QFile f("/etc/encsoft/dbfolder/MediaCommon.json");
	f.open(QIODevice::ReadOnly);
	const QJsonObject &media = QJsonDocument::fromJson(f.readAll()).object();
	f.close();
	if (media.isEmpty())
		return false;

	QJsonArray encoders = media["VideoEncoders"].toObject()["Configurations"].toArray();
	for (int i = 0; i < encoders.size(); i++) {
		QJsonObject enc = encoders[i].toObject();
		QString token = enc["_attrs"].toArray().first().toObject()["value"].toString();
		QJsonObject mcast = enc["Multicast"].toObject();
		RtpTransmitter *rtp = NULL;
		if (!mcast["AutoStart"].toBool())
			continue;
		if (token == "VideoEncoder0")
			rtp = rtsp->getSessionTransmitter("stream1m", "");
		else if (token == "VideoEncoder1")
			rtp = rtsp->getSessionTransmitter("stream3m", "");
		if (!rtp)
			continue;

		QString target = mcast["Address"].toObject()["IPv4Address"].toString();
		quint16 dport = mcast["Port"].toInt();
		int srcport, srcport2;
		rtsp->detectLocalPorts(srcport, srcport2);
		uint ssrc = 0xdeadbeaf;

		ffDebug() << "starting multicast streaming" << token << target << dport << srcport;
		RtpChannel *ch = rtp->addChannel();
		rtp->setupChannel(ch, target, dport, dport + 1, srcport, srcport2, ssrc);
		rtp->playChannel(ch);
	}

	return false;
}

void GenericStreamer::initOnvifBindings()
{
	onvifWatcher = new QFileSystemWatcher(this);
	connect(onvifWatcher, SIGNAL(fileChanged(QString)), SLOT(onvifChanged(QString)));
	onvifWatcher->addPath("/etc/encsoft/dbfolder/MediaCommon.json");
}

TextOverlay * GenericStreamer::createTextOverlay(const QString &elementName, ApplicationSettings *s)
{
	/* overlay element, only for High res channel */
	TextOverlay *textOverlay = new TextOverlay(TextOverlay::PIXEL_MAP);
	textOverlay->setObjectName(elementName);
	/* overlay settings */
	textOverlay->setEnabled(s->get("text_overlay.enabled").toBool());
	QPoint pt;
	pt.setX(s->get("text_overlay.position.x").toInt());
	pt.setY(s->get("text_overlay.position.y").toInt());
	textOverlay->setOverlayPosition(pt);
	textOverlay->setFontSize(s->get("text_overlay.font_size").toInt());
	textOverlay->setOverlayText(s->get("text_overlay.overlay_text").toString());
	textOverlay->setOverlayTimeZone(s->get("text_overlay.overlay_timezone").toInt());
	textOverlay->setOverlayDateFormat(s->get("text_overlay.overlay_date_format").toString());
	textOverlay->setClearBackground(s->get("text_overlay.clear_background").toInt());
	int cnt = s->get("text_overlay.fields.count").toInt();
	for (int i = 0; i < cnt; i++) {
		QString pre = QString("text_overlay.field.%1").arg(i);
		textOverlay->addOverlayField((TextOverlay::overlayTextFields)(s->get(pre + ".type").toInt()),
									 s->get(pre + ".text").toString());
	}
	return textOverlay;
}

H264Encoder * GenericStreamer::createH264Encoder(const QString &elementName, ApplicationSettings *s, int ch, int w0, int h0)
{
	H264Encoder *enc = new H264Encoder;
	enc->setObjectName(elementName);
	/* first encoder settings */
	int rateControl = s->get(QString("video_encoding.ch.%1.rate_control").arg(ch)).toInt();
	int bufCnt = s->get(QString("video_encoding.ch.%1.buffer_count").arg(ch)).toInt();
	int bitrate = s->get(QString("video_encoding.ch.%1.bitrate").arg(ch)).toInt();
	int profile = s->get(QString("video_encoding.ch.%1.encoding_profile").arg(ch)).toInt();
	enc->setBitrateControl(DmaiEncoder::RateControl(rateControl));
	enc->setBitrate(w0 * h0 * getVideoFps(ch) * 3 * 0.07);
	if (bitrate)
		enc->setBitrate(bitrate);
	enc->setParameter("videoWidth", w0);
	enc->setParameter("videoHeight", h0);
	enc->setBufferCount(bufCnt);
	enc->setProfile(profile);
	/* we support SEI at RTP level, so bypassing encoder support */
	enc->setSeiEnabled(false);
	enc->setPacketized(s->get(QString("video_encoding.ch.%1.packetized").arg(ch)).toBool());
	enc->setFrameRate(getVideoFps(ch));
	enc->setIntraFrameInterval(s->get(QString("video_encoding.ch.%1.intraframe_interval").arg(ch)).toInt());
	if (s->get(QString("video_encoding.ch.%1.motion_detection").arg(ch)).toInt())
		enc->setMotionVectorExtraction(H264Encoder::MV_MOTDET);
	else
		enc->setMotionVectorExtraction(H264Encoder::MV_NONE);
	enc->setMotionDetectionThreshold(s->get(QString("video_encoding.ch.%1.motion_detection_threshold").arg(ch)).toInt());
	enc->setMotionSensitivity(s->get(QString("video_encoding.ch.%1.motion_sensitivity").arg(ch)).toInt());
	enc->setTrainingSample(s->get(QString("video_encoding.ch.%1.training_sample_number").arg(ch)).toInt());
	enc->setLearningCoef(s->get(QString("video_encoding.ch.%1.learning_coef").arg(ch)).toInt());
	enc->setVarianceOffset(s->get(QString("video_encoding.ch.%1.variance_offset").arg(ch)).toInt());
	return enc;
}

JpegEncoder * GenericStreamer::createJpegEncoder(const QString &elementName, ApplicationSettings *s, int ch, int w0, int h0)
{
	JpegEncoder *enc = new JpegEncoder;
	enc->setObjectName(elementName);
	enc->setQualityFactor(s->get(QString("jpeg_encoding.ch.%1.qfact").arg(ch)).toInt());
	enc->setMaxJpegSize(s->get(QString("jpeg_encoding.ch.%1.max_size").arg(ch)).toInt());
	enc->setParameter("videoWidth", w0);
	enc->setParameter("videoHeight", h0);
	return enc;
}

QString GenericStreamer::getProcessName()
{
	return "dm365_ipstr";
}

int GenericStreamer::getInt(const QString &fld)
{
	if (fld == "wd_key")
		return getWdogKey();
	if (fld.startsWith("rtsp.stats"))
		return getRtspStats(fld).toInt();
	return ApplicationSettings::instance()->get(fld).toInt();
}

QString GenericStreamer::getString(const QString &fld)
{
	if (fld.startsWith("rtsp.stats"))
		return getRtspStats(fld).toString();
	return ApplicationSettings::instance()->get(fld).toString();
}

QVariant GenericStreamer::getVariant(const QString &fld)
{
	if (fld.startsWith("rtsp.stats"))
		return getRtspStats(fld);
	return ApplicationSettings::instance()->get(fld);
}

int GenericStreamer::setInt(const QString &fld, qint32 val)
{
	return ApplicationSettings::instance()->set(fld, val);
}

int GenericStreamer::setString(const QString &fld, const QString &val)
{
	if (fld == "_overlay.enabled") {
		if (val == "true")
			mainTextOverlay->setEnabled(true);
		else
			mainTextOverlay->setEnabled(false);
		return 0;
	} else if (fld.startsWith("_overlay.")) {
		int pos = fld.split(".").last().toInt();
		return mainTextOverlay->setOverlayFieldText(pos, val);
	}
	return ApplicationSettings::instance()->set(fld, val);
}

int GenericStreamer::setVariant(const QString &fld, const QVariant &val)
{
	return ApplicationSettings::instance()->set(fld, val);
}
