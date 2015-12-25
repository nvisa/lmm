#include "testcontroller.h"
#include "btcpserver.h"

#include <QTimer>
#include <QTcpSocket>
#include <QDataStream>
#include <QMutexLocker>

#include <lmm/debug.h>
#include <lmm/streamtime.h>
#include <lmm/baselmmelement.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/pipeline/pipelinemanager.h>
#include <lmm/pipeline/pipelinedebugger.h>

#define dsInit() \
	QByteArray ba; \
	QDataStream out(&ba, QIODevice::WriteOnly); \
	out.setByteOrder(QDataStream::LittleEndian); \
	out.setFloatingPointPrecision(QDataStream::SinglePrecision);

#define dsInitR() \
	QDataStream in(data); \
	in.setByteOrder(QDataStream::LittleEndian); \
	in.setFloatingPointPrecision(QDataStream::SinglePrecision);

enum Commands {
	CMD_GET_INFO,
	CMD_STOP_TEST,

	CMD_INFO,
};

TestController * TestController::inst = NULL;

TestController::TestController() :
	QObject()
{
	timer = new QTimer(this);
	//timer->start(1000);
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	targetFrameCount = 0;
	stopTestRecved = false;

	BTcpServer *serv = new BTcpServer(this);
	serv->listen(QHostAddress::Any, 51234);
	connect(serv, SIGNAL(newDataAvailable(QTcpSocket*,QByteArray)), SLOT(newTcpDataAvailable(QTcpSocket*,QByteArray)));
}

TestController * TestController::instance()
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

void TestController::OnTestStart(const testing::TestInfo &info)
{
	QMutexLocker locker(&ci.l);
	ci.currentTestName = QString::fromUtf8(info.name());
	timer->start(1000);
}

void TestController::OnTestEnd(const testing::TestInfo &)
{
}

void TestController::OnTestCaseStart(const testing::TestCase &info)
{
	QMutexLocker locker(&ci.l);
	ci.currentTestCaseName = QString::fromUtf8(info.name());
}

void TestController::OnTestCaseEnd(const testing::TestCase &)
{

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
		if ((targetFrameCount && stats.outCount > targetFrameCount) || stopTestRecved) {
			man->stop();
			timer->stop();
			break;
		}
	}
}

void TestController::newTcpDataAvailable(QTcpSocket *sock, const QByteArray &data)
{
	dsInitR();
	qint32 cmd; in >> cmd;
	dsInit();
	if (cmd == CMD_GET_INFO) {
		out << (qint32)CMD_INFO;

		QMutexLocker locker(&ci.l);
		out << ci.currentTestCaseName;
		out << ci.currentTestName;

		PipelineDebugger *dbg = PipelineDebugger::GetInstance();
		qint32 pcnt = dbg->getPipelineCount();
		out << pcnt;
		for (int j = 0; j < pcnt; j++) {
			BaseLmmPipeline *pl = dbg->getPipeline(j);
			const BaseLmmPipeline::PipelineStats stats = pl->getStats();
			out << stats.lastStreamBufferNo;
			out << stats.outCount;
			out << pl->getStreamTime()->getFreeRunningTime();
			out << (qint32)pl->getStreamTime()->getElapsedWallTime();

			out << (qint32)pl->getPipeCount();
			for (int i = 0; i < pl->getPipeCount(); i++) {
				BaseLmmElement *el = pl->getPipe(i);
				out << (qint32)el->getOutputQueue(0)->getFps();
			}
		}
	} else if (cmd == CMD_STOP_TEST) {
		stopTestRecved = true;
	}
	if (ba.size())
		BTcpServer::send(ba, sock);
}
