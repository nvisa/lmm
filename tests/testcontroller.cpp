#include "testcontroller.h"

#include <QTimer>

#include <lmm/debug.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/pipeline/pipelinemanager.h>
#include <lmm/pipeline/pipelinedebugger.h>

TestController * TestController::inst = NULL;

TestController::TestController() :
	QObject()
{
	timer = new QTimer(this);
	timer->start(1000);
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	targetFrameCount = 0;
}

TestController *TestController::instance()
{
	if (!inst)
		inst = new TestController;
	return inst;
}

void TestController::setTargetFrameCount(int value)
{
	targetFrameCount = value;
}

void TestController::registerManager(PipelineManager *man)
{
	managers << man;
}

void TestController::timeout()
{
	const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
	if (!info)
		return;
	//const testing::TestCase *tcase = testing::UnitTest::GetInstance()->current_test_case();
	//QString cname = QString("%1.%2").arg(info->test_case_name()).arg(info->name());
	PipelineDebugger *dbg = PipelineDebugger::GetInstance();
	if (!dbg->getPipelineCount())
		return;

	int pcnt = dbg->getPipelineCount();
	for (int j = 0; j < pcnt; j++) {
		BaseLmmPipeline *pl = dbg->getPipeline(j);
		PipelineManager *man = qobject_cast<PipelineManager *>(pl->parent());
		if (!man)
			continue;
		const BaseLmmPipeline::PipelineStats stats = pl->getStats();
		if (targetFrameCount && stats.outCount > targetFrameCount) {
			man->stop();
			break;
		}
	}
}
