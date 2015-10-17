#ifndef BASEPIPEELEMENT_H
#define BASEPIPEELEMENT_H

#include <lmm/baselmmelement.h>

#include <QStringList>

class RateReducto;
class BaseLmmPipeline;

typedef void (*pipeHook)(const RawBuffer &, void *);

class BasePipeElement : public BaseLmmElement
{
	Q_OBJECT
public:
	class Link {
	public:
		Link()
		{
			reducto = NULL;
			source = NULL;
			destination = NULL;
		}

		BaseLmmElement *source;
		BaseLmmElement *destination;
		int sourceChannel;
		int destinationChannel;
		int sourceProcessChannel;
		RateReducto *reducto;

		int addBuffer(int ch, const RawBuffer &buffer);
		int addBuffer(const RawBuffer &buffer);
		int setRateReduction(int skip, int outOf);
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
	void setLink(const Link &link) { this->link = link; }
	void addNewLink(const Link &link);
	void setCopyOnUse(bool v) { copyOnUse = v; }
	void addOutputHook(pipeHook hook, void *priv);
	int setRateReduction(int skip, int outOf);
	void addPassFilter(const QString &mimeType);

	virtual void threadFinished(LmmThread *);

	const Link & getLink() const { return link; }
protected:
	int processBuffer(const RawBuffer &buf);

	BaseLmmPipeline *pipeline;
	Link link;
	QList<Link> copyLinks;
	bool copyOnUse;
	pipeHook outHook;
	void *outPriv;
	QStringList filterMimes;
};

#endif // BASEPIPEELEMENT_H
