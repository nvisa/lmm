#ifndef CAMERADEVICESOURCE_H
#define CAMERADEVICESOURCE_H

#include "rawbuffer.h"

#include <QList>
#include <QMutex>

#include <FramedSource.hh>

class BaseLmmElement;

// The following class can be used to define specific encoder parameters
class DeviceParameters {
	//%%% TO BE WRITTEN %%%
};

class CameraDeviceSource : public FramedSource
{
public:
	static CameraDeviceSource* createNew(UsageEnvironment& env,
										 DeviceParameters params);
	int addBuffer(RawBuffer buf);
	void setSourceElement(BaseLmmElement *el) { sourceElement = el; }

public:
	static TaskScheduler *scheduler;
	static EventTriggerId eventTriggerId;
	static CameraDeviceSource *instance;

protected:
	CameraDeviceSource(UsageEnvironment& env, DeviceParameters params);
	// called only by createNew(), or by subclass constructors
	virtual ~CameraDeviceSource();

private:
	// redefined virtual functions:
	virtual void doGetNextFrame();

private:
	static void deliverFrame0(void* clientData);
	void deliverFrame();

private:
	static unsigned referenceCount; // used to count how many instances of this class currently exist
	DeviceParameters fParams;

	QList<RawBuffer> inputBuffers;
	QMutex lock;
	BaseLmmElement *sourceElement;
	RawBuffer buffer;
	int bufferOffset;
};

#endif // DEVICESOURCE_H
