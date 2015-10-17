#ifndef PIPELINEDEBUGGER_H
#define PIPELINEDEBUGGER_H

#include <QHash>
#include <QObject>

class QUdpSocket;
class RemotePipeline;
class BaseLmmPipeline;

class PipelineDebugger : public QObject
{
	Q_OBJECT
public:
	enum Commands {
		CMD_GET_PIPELINE_COUNT,
		CMD_GET_DESCR,
		CMD_GET_INFO,

		CMD_INFO_PIPELINE_COUNT,
		CMD_INFO_DESCR,
	};

	explicit PipelineDebugger(QObject *parent = 0);

	void addPipeline(BaseLmmPipeline *pipeline);
signals:

public slots:
	void udpDataReady();
	const QByteArray processDatagram(const QByteArray &ba);

protected:
	QUdpSocket *sock;
	QList<BaseLmmPipeline *> pipelines;
};

#endif // PIPELINEDEBUGGER_H
