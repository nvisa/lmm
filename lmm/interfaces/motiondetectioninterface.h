#ifndef MOTIONDETECTIONINTERFACE_H
#define MOTIONDETECTIONINTERFACE_H

class MotionDetectionInterface
{
public:

	virtual int getMotionValue() = 0;
	virtual int getMotionRegions() = 0;
};

#endif // MOTIONDETECTIONINTERFACE_H
