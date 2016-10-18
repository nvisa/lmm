#ifndef STREAMCONTROLELEMENTINTERFACE_H
#define STREAMCONTROLELEMENTINTERFACE_H

class StreamControlElementInterface
{
public:
	StreamControlElementInterface();

	virtual bool isActive() = 0;
};

#endif // STREAMCONTROLELEMENTINTERFACE_H
