#ifndef BASEPIPEELEMENT_H
#define BASEPIPEELEMENT_H

#include <lmm/baselmmelement.h>

class BaseLmmPipeline;

class BasePipeElement : public BaseLmmElement
{
	Q_OBJECT
public:
	struct pipe {
		BaseLmmElement *source;
		BaseLmmElement *destination;
		int sourceChannel;
		int destinationChannel;
		int sourceProcessChannel;
	};
	struct outLink {
		BaseLmmElement *destination;
		int destinationChannel;
	};

	explicit BasePipeElement(BaseLmmPipeline *parent = 0);

	int operationProcess();
	int operationBuffer();

	virtual int addBuffer(int ch, const RawBuffer &buffer);

	void setPipe(BaseLmmElement *target, BaseLmmElement *next, int tch = 0, int pch = 0, int nch = 0);
	void setNextChannel(int ch) { link.destinationChannel = ch; }
	void setNext(BaseLmmElement *el) { link.destination = el; }
	void setProcessChannel(int ch) { link.sourceProcessChannel = ch; }
	void setSource(BaseLmmElement *el) { link.source = el; }
	void setSourceChannel(int ch) { link.sourceChannel = ch; }
	void setLink(const pipe &link) { this->link = link; }
	void addNewLink(const outLink &link);

	virtual void threadFinished(LmmThread *);

	const struct pipe getLink() const { return link; }
protected:
	int processBuffer(const RawBuffer &buf);

	BaseLmmPipeline *pipeline;
	pipe link;
	QList<outLink> copyLinks;
	
};

#endif // BASEPIPEELEMENT_H