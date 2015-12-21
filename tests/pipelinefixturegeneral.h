#ifndef PIPELINEFIXTUREGENERAL_H
#define PIPELINEFIXTUREGENERAL_H

#include <gtest/gtest.h>

#include <lmm/pipeline/pipelinemanager.h>

#include <QHash>
#include <QList>

class BaseLmmPipeline;

class PipelineTestPars
{
public:
	PipelineTestPars()
	{
		targetFrameCount = 0;
		targetFrameWidth = 0;
		targetFrameHeight = 0;
		h264EncoderBufferCount = 5;
		targetFps = 30;
	}

	/* target values */
	int targetFps;
	int targetFrameCount;
	int targetFrameWidth;
	int targetFrameHeight;
	int h264EncoderBufferCount;
};

class PipelineFixtureGeneral : public testing::TestWithParam<std::vector<PipelineTestPars> >, public PipelineManager
{
public:
	class TestStats
	{
	public:
		TestStats()
		{
			outCount = 0;
			lastFrameWidth = 0;
			lastFrameHeight = 0;
		}

		int outCount;
		int lastFrameWidth;
		int lastFrameHeight;
		int lastFrameSize;
	};

	void setupPipeline(int ptype);

	int createCapturePipeline1();
	int createEncodePipeline1();
	int createEncodePipeline2();
	int createEncodePipeline3();

	QHash<BaseLmmPipeline *, TestStats> tstats;
	QList<PipelineTestPars> ppars;

protected:
	PipelineFixtureGeneral()
		: PipelineManager()
	{

	}
	virtual ~PipelineFixtureGeneral() {}

	virtual int pipelineOutput(BaseLmmPipeline *pl, const RawBuffer &buf);
};

#endif // PIPELINEFIXTUREGENERAL_H
