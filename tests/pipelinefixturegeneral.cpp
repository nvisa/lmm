#include "pipelinefixturegeneral.h"
#include "testcontroller.h"

#include <lmm/baselmmelement.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/dm365/dm365camerainput.h>
#include <lmm/pipeline/pipelinemanager.h>

#include <QSettings>

#define ptstat(__x) tstats[getPipeline(__x)]

/*
 * Adding a new parameter:
 *		1. Add it into PipelineTestPars structure.
 *		2. Read from settings file in getTestPars() function.
 *		3.
 */

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
	pars[1].targetFrameCount = pars[0].targetFrameCount;
	pars[1].targetFrameWidth = sets.value("targetFrameWidth1").toInt();
	pars[1].targetFrameHeight = sets.value("targetFrameWidth1").toInt();

	sets.endArray();
	sets.endGroup();

	return pars;
}

void PipelineFixtureGeneral::setupParameters()
{
	/* adjust run-time parameters */
	const std::vector<PipelineTestPars> &plist = GetParam();
	for (uint i = 0; i < plist.size(); i++) {
		ppars << PipelineTestPars();
		ppars[i] = plist[i];
	}

	/* set controller's frame count so that test stops after target frame count */
	TestController::instance()->setTargetFrameCount(ppars[0].targetFrameCount);
}

int PipelineFixtureGeneral::createCapturePipeline1()
{
	DM365CameraInput *camIn = new DM365CameraInput();
	camIn->setSize(0, QSize(ppars[0].targetFrameWidth, ppars[0].targetFrameHeight));
	camIn->setSize(1, QSize(ppars[1].targetFrameWidth, ppars[1].targetFrameHeight));
	BaseLmmPipeline *p1 = addPipeline();
	p1->append(camIn);
	p1->end();

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(camIn);
	p2->end();

	return 0;
}

int PipelineFixtureGeneral::pipelineOutput(BaseLmmPipeline *pl, const RawBuffer &buf)
{
	tstats[pl].outCount++;
	if (buf.constPars()->videoWidth)
		tstats[pl].lastFrameWidth = buf.constPars()->videoWidth;
	if (buf.constPars()->videoHeight)
		tstats[pl].lastFrameHeight = buf.constPars()->videoHeight;
	return 0;
}

TEST_P(PipelineFixtureGeneral, CaptureTest) {
	setupParameters();

	/* create capture pipeline */
	createCapturePipeline1();

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

INSTANTIATE_TEST_CASE_P(SomeInstantName, PipelineFixtureGeneral, testing::Values(getTestPars("FunctionalityTests", 0)));
