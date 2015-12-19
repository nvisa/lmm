#ifndef TESTCONTROLLER_H
#define TESTCONTROLLER_H

#include <QObject>

#include <gtest/gtest.h>

class QTimer;
class BaseLmmPipeline;
class PipelineManager;


class TestController : public QObject, public testing::EmptyTestEventListener
{
	Q_OBJECT
public:

	static TestController * instance();
	void setTargetFrameCount(int value);
	void registerManager(PipelineManager *man);

	/* gtest event handlers */
#if 0
	virtual void OnTestStart(const testing::TestInfo& /*test_info*/);
	virtual void OnTestEnd(const testing::TestInfo& /*test_info*/);
	virtual void OnTestProgramStart(const UnitTest& /*unit_test*/) {}
	virtual void OnTestIterationStart(const UnitTest& /*unit_test*/,
									  int /*iteration*/) {}
	virtual void OnEnvironmentsSetUpStart(const UnitTest& /*unit_test*/) {}
	virtual void OnEnvironmentsSetUpEnd(const UnitTest& /*unit_test*/) {}
	virtual void OnTestCaseStart(const TestCase& /*test_case*/) {}
	virtual void OnTestPartResult(const TestPartResult& /*test_part_result*/) {}
	virtual void OnTestCaseEnd(const TestCase& /*test_case*/) {}
	virtual void OnEnvironmentsTearDownStart(const UnitTest& /*unit_test*/) {}
	virtual void OnEnvironmentsTearDownEnd(const UnitTest& /*unit_test*/) {}
	virtual void OnTestIterationEnd(const UnitTest& /*unit_test*/,
									int /*iteration*/) {}
	virtual void OnTestProgramEnd(const UnitTest& /*unit_test*/) {}
#endif
signals:

protected slots:
	void timeout();

protected:
	explicit TestController();

	QTimer *timer;
	QList<PipelineManager *> managers;
	int targetFrameCount;
	static TestController *inst;
};

#endif // TESTCONTROLLER_H
