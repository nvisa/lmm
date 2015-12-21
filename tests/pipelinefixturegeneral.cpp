#include "pipelinefixturegeneral.h"
#include "testcontroller.h"

#include <lmm/debug.h>
#include <lmm/baselmmelement.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/dmai/h264encoder.h>
#include <lmm/dmai/videotestsource.h>
#include <lmm/dm365/dm365camerainput.h>
#include <lmm/pipeline/pipelinemanager.h>

#include <QSettings>

/*
 * Adding a new parameter:
 *		1. Add it into PipelineTestPars structure.
 *		2. Read from settings file in getTestPars() function.
 */

#define ptstat(__x) tstats[getPipeline(__x)]
#define INST_TEST_CASE(_name, _x) \
	INSTANTIATE_TEST_CASE_P(PipelineTestInstance##_x, PipelineFixtureGeneral, testing::Values(getTestPars(_name, _x)));

static inline void setElSize(BaseLmmElement *el, QSize sz)
{
	el->setParameter("videoWidth", sz.width());
	el->setParameter("videoHeight", sz.height());
}

static std::vector<PipelineTestPars> getTestPars(const QString &group, int parset)
{
	std::vector<PipelineTestPars> pars;
	pars.push_back(PipelineTestPars());
	pars.push_back(PipelineTestPars());

	QSettings sets("testpars.ini", QSettings::IniFormat);
	sets.beginGroup(group);
	sets.beginReadArray("parsets");
	sets.setArrayIndex(parset);

	pars[0].targetFrameCount = sets.value("targetFrameCount").toInt();
	pars[0].targetFrameWidth = sets.value("targetFrameWidth0").toInt();
	pars[0].targetFrameHeight = sets.value("targetFrameHeight0").toInt();
	pars[0].h264EncoderBufferCount = sets.value("h264EncoderBufferCount0").toInt();
	pars[0].targetFps = sets.value("targetFps0").toInt();
	pars[1].targetFrameCount = pars[0].targetFrameCount;
	pars[1].targetFrameWidth = sets.value("targetFrameWidth1").toInt();
	pars[1].targetFrameHeight = sets.value("targetFrameWidth1").toInt();

	/* global parameters are kept in first pipeline */

	sets.endArray();
	sets.endGroup();

	return pars;
}

void PipelineFixtureGeneral::setupPipeline(int ptype)
{
	/* adjust run-time parameters */
	const std::vector<PipelineTestPars> &plist = GetParam();
	for (uint i = 0; i < plist.size(); i++) {
		ppars << PipelineTestPars();
		ppars[i] = plist[i];
	}

	/* set controller's frame count so that test stops after target frame count */
	TestController::instance()->setTargetFrameCount(ppars[0].targetFrameCount);

	/* create test pipeline */
	if (ptype == 0)
		createCapturePipeline1();
	else if (ptype == 1)
		createEncodePipeline1();
	else if (ptype == 2)
		createEncodePipeline2();
	else if (ptype == 3)
		createEncodePipeline3();
	else
		return;

	for (int i = 0; i < getPipelineCount(); i++)
		getPipeline(i)->end();
}

int PipelineFixtureGeneral::createCapturePipeline1()
{
	DM365CameraInput *camIn = new DM365CameraInput();
	camIn->setSize(0, QSize(ppars[0].targetFrameWidth, ppars[0].targetFrameHeight));
	camIn->setSize(1, QSize(ppars[1].targetFrameWidth, ppars[1].targetFrameHeight));
	BaseLmmPipeline *p1 = addPipeline();
	p1->append(camIn);

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(camIn);

	return 0;
}

int PipelineFixtureGeneral::createEncodePipeline1()
{
	DM365CameraInput *camIn = new DM365CameraInput();
	camIn->setSize(0, QSize(ppars[0].targetFrameWidth, ppars[0].targetFrameHeight));
	camIn->setSize(1, QSize(ppars[1].targetFrameWidth, ppars[1].targetFrameHeight));
	QSize sz0 = camIn->getSize(0);
	int videoFps = 30;

	H264Encoder *enc264High = new H264Encoder;
	enc264High->setSeiEnabled(true);
	enc264High->setPacketized(false);
	enc264High->setObjectName("H264EncoderHigh");
	/*
	 *  We set bitrate according to suggestion presented at [1].
	 *  For 1280x720@30fps this results something like 6 mbit.
	 *  For 320x240@20fps this results something like 500 kbit.
	 *
	 *  We take motion rate as 3 which can be something between 1-4.
	 *
	 * [1] http://stackoverflow.com/questions/5024114/suggested-compression-ratio-with-h-264
	 */
	enc264High->setBitrateControl(DmaiEncoder::RATE_VBR);
	enc264High->setBitrate(sz0.width() * sz0.height() * videoFps * 3 * 0.07);
	/*
	 *  Profile 2 is derived from TI recommendations for video surveillance
	 */
	enc264High->setProfile(2);
	enc264High->setBufferCount(ppars[0].h264EncoderBufferCount);
	setElSize(enc264High, sz0);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(camIn);
	p1->append(enc264High);

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(camIn);

	return 0;
}

int PipelineFixtureGeneral::createEncodePipeline2()
{
	createEncodePipeline1();

	DM365CameraInput *camIn = (DM365CameraInput *)getPipeline(1)->getPipe(0);
	int videoFps = 30;
	QSize sz1 = camIn->getSize(1);

	H264Encoder*enc264Low = new H264Encoder;
	enc264Low->setSeiEnabled(false);
	enc264Low->setPacketized(false);
	enc264Low->setObjectName("H264EncoderLow");
	enc264Low->setBitrateControl(DmaiEncoder::RATE_VBR);
	enc264Low->setBitrate(sz1.width() * sz1.height() * videoFps * 3 * 0.07);
	enc264Low->setProfile(0);
	setElSize(enc264Low, sz1);

	BaseLmmPipeline *p2 = getPipeline(1);
	p2->append(enc264Low);

	return 0;
}

int PipelineFixtureGeneral::createEncodePipeline3()
{
	QSize sz0(ppars[0].targetFrameWidth, ppars[0].targetFrameHeight);
	VideoTestSource *tsrc = new VideoTestSource;
	setElSize(tsrc, sz0);
	tsrc->setTestPattern(VideoTestSource::COLORBARS);
	tsrc->setFps(ppars[0].targetFps);

	H264Encoder *enc264High = new H264Encoder;
	enc264High->setFrameRate(ppars[0].targetFps < 120 ? ppars[0].targetFps : 120);
	enc264High->setSeiEnabled(false);
	enc264High->setPacketized(false);
	enc264High->setObjectName("H264EncoderHigh");
	enc264High->setProfile(0);
	enc264High->setBufferCount(ppars[0].h264EncoderBufferCount);
	setElSize(enc264High, sz0);

	H264Encoder *enc264High2 = new H264Encoder;
	enc264High2->setFrameRate(ppars[0].targetFps < 120 ? ppars[0].targetFps : 120);
	enc264High2->setSeiEnabled(false);
	enc264High2->setPacketized(false);
	enc264High2->setObjectName("H264EncoderHigh2");
	enc264High2->setProfile(0);
	enc264High2->setBufferCount(ppars[0].h264EncoderBufferCount);
	setElSize(enc264High2, sz0);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(tsrc);
	p1->append(enc264High);
#if 0
	BaseLmmPipeline *p2 = addPipeline();
	enc264High2->addInputQueue(tsrc->getOutputQueue(0));
	p2->append(enc264High2);
#endif

	return 0;
}

int PipelineFixtureGeneral::pipelineOutput(BaseLmmPipeline *pl, const RawBuffer &buf)
{
	tstats[pl].outCount++;
	if (buf.constPars()->videoWidth)
		tstats[pl].lastFrameWidth = buf.constPars()->videoWidth;
	if (buf.constPars()->videoHeight)
		tstats[pl].lastFrameHeight = buf.constPars()->videoHeight;
	if (buf.size())
		tstats[pl].lastFrameSize = buf.size();
	return 0;
}

TEST_P(PipelineFixtureGeneral, CaptureTest) {
	setupPipeline(0);

	/* start pipeline and wait till finished */
	start();
	while (isRunning()) {}

	/* check return values */
	EXPECT_GE(ptstat(0).outCount, ppars[0].targetFrameCount);
	EXPECT_GE(ptstat(1).outCount, ppars[1].targetFrameCount);
	EXPECT_EQ(ptstat(0).lastFrameWidth, ppars[0].targetFrameWidth);
	EXPECT_EQ(ptstat(0).lastFrameHeight, ppars[0].targetFrameHeight);
	EXPECT_EQ(ptstat(1).lastFrameWidth, ppars[1].targetFrameWidth);
	EXPECT_EQ(ptstat(1).lastFrameHeight, ppars[1].targetFrameHeight);
}

TEST_P(PipelineFixtureGeneral, EncodeTest1) {
	setupPipeline(1);

	/* start pipeline and wait till finished */
	start();
	while (isRunning()) {}

	/* check return values */
	EXPECT_GE(ptstat(0).outCount, ppars[0].targetFrameCount);
	EXPECT_GE(ptstat(1).outCount, ppars[1].targetFrameCount);
	EXPECT_LT(ptstat(0).lastFrameSize, 300 * 1024);
}

TEST_P(PipelineFixtureGeneral, EncodeTest2) {
	setupPipeline(2);

	/* start pipeline and wait till finished */
	start();
	while (isRunning()) {}

	/* check return values */
	EXPECT_GE(ptstat(0).outCount, ppars[0].targetFrameCount);
	EXPECT_GE(ptstat(1).outCount, ppars[1].targetFrameCount);
	EXPECT_LT(ptstat(0).lastFrameSize, 300 * 1024);
	EXPECT_LT(ptstat(1).lastFrameSize, 75 * 1024);
	EXPECT_GT(getPipeline(0)->getPipe(0)->getOutputQueue(0)->getFps(), ppars[0].targetFps);
}

TEST_P(PipelineFixtureGeneral, EncodeTest3) {
	ASSERT_TRUE(QFile::exists("/etc/lmm/patterns/720p/colorbars.png"));

	setupPipeline(3);

	/* start pipeline and wait till finished */
	start();
	while (isRunning()) {}

	/* check return values */
	EXPECT_GE(ptstat(0).outCount, ppars[0].targetFrameCount);
	EXPECT_EQ(ptstat(0).lastFrameWidth, ppars[0].targetFrameWidth);
	EXPECT_EQ(ptstat(0).lastFrameHeight, ppars[0].targetFrameHeight);
	int fpsTotal = 0;
	for (int i = 0; i < getPipelineCount(); i++)
		fpsTotal += getPipeline(i)->getOutputQueue(0)->getFps();
	EXPECT_GT(fpsTotal, ppars[0].targetFps);
}

INST_TEST_CASE("FunctionalityTests", 0);
//INST_TEST_CASE("FunctionalityTests", 1);
