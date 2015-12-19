#include "testcontroller.h"
#include "pipelinefixturegeneral.h"

#include <gtest/gtest.h>

#include <lmm/debug.h>
#include <lmm/lmmcommon.h>
#include <lmm/tools/cpuload.h>
#include <lmm/baselmmelement.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/pipeline/pipelinemanager.h>

#include <lmm/dm365/dm365camerainput.h>

#include <QList>
#include <QThread>
#include <QSettings>
#include <QCoreApplication>

#include <errno.h>

class TestThread : public QThread
{
public:
	void run()
	{
		QThread::sleep(1);
		int err = RUN_ALL_TESTS();
		qDebug() << "tests finished with err" << err;
	}
};

int main(int argc, char **argv)
{
	QCoreApplication a(argc, argv);

	if (a.arguments().contains("--create-sample-def")) {
		QSettings sets("testpars.ini", QSettings::IniFormat);
		sets.beginGroup("FunctionalityTests");
		sets.beginWriteArray("parsets");
		for (int i = 0; i < 2; i++) {
			sets.setArrayIndex(i);
			sets.setValue("targetFrameCount", 10);
			sets.setValue("targetFrameWidth0", 1280);
			sets.setValue("targetFrameHeight0", 720);
			sets.setValue("targetFrameWidth1", 352);
			sets.setValue("targetFrameHeight1", 288);
			sets.setValue("h264EncoderBufferCount0", 10);
			sets.setValue("h264EncoderBufferCount1", 10);

			/* global parameters */
			sets.setValue("pipelineType", i);
		}
		sets.endArray();
		sets.endGroup();
		return 0;
	}
	testing::InitGoogleTest(&argc, argv);

	if (a.arguments().contains("--help") || a.arguments().contains("-h"))
		return 0;

	testing::UnitTest::GetInstance()->listeners().Append(TestController::instance());
	LmmCommon::init();

	TestThread t;
	t.start();
	return a.exec();
}

