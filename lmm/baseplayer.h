#ifndef BASEPLAYER_H
#define BASEPLAYER_H

#include <lmm/baselmmelement.h>
#include <lmm/lmmcommon.h>
#include <lmm/lmmthread.h>

#include <QUrl>
#include <QTimer>

#define createOpThread(__func, __name, __class) { \
	LmmThread *th = new OpThread<__class>(this, __func, __name); \
	threads.insert(__name, th); \
	th->start(); \
	}

#define createOpThreadPri(__func, __name, __class, __pri) { \
	LmmThread *th = new OpThread<__class>(this, __func, __name); \
	threads.insert(__name, th); \
	th->start(__pri); \
	}

template <class T>
class OpThread : public LmmThread
{
public:
	typedef int (T::*threadOp)();
	OpThread(T *parent, threadOp op, QString name)
		: LmmThread(name, parent)
	{
		enc = parent;
		mfunc = op;
	}
	OpThread(T *parent, threadOp op, QString name, BaseLmmElement *parElement)
		: LmmThread(name, parElement)
	{
		enc = parent;
		mfunc = op;
	}
	int operation()
	{
		return (enc->*mfunc)();
	}

private:
	T *enc;
	threadOp mfunc;
};

template <class T>
class OpThread2 : public LmmThread
{
public:
	typedef int (T::*threadOp)(int);
	OpThread2(T *parent, threadOp op, QString name, int idx)
		: LmmThread(name, parent)
	{
		enc = parent;
		mfunc = op;
		val = idx;
	}
	OpThread2(T *parent, threadOp op, QString name, int idx, BaseLmmElement *parElement)
		: LmmThread(name, parElement)
	{
		enc = parent;
		mfunc = op;
		val = idx;
	}
	int operation()
	{
		return (enc->*mfunc)(val);
	}

private:
	int val;
	T *enc;
	threadOp mfunc;
};

class BasePlayer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BasePlayer(QObject *parent = 0);
	virtual int start();
	virtual int stop();
	virtual int pause();
	virtual int resume();
	virtual qint64 getDuration();
	virtual qint64 getPosition();
	virtual int seekTo(qint64 pos);
	virtual int seek(qint64 value);
	virtual int setMute(bool mute);
	virtual int setVolumeLevel(int per);
	virtual int getVolumeLevel();
	virtual bool isEOF();
	virtual int setSourceUrl(QUrl url);
	virtual QUrl getSourceUrl() { return sourceUrl; }
	virtual int setFilename(QString filename);
	StreamTime * getStreamTime() { return streamTime; }
signals:
	void playbackFinished(int err);
public slots:
	virtual void timeout();
protected:
	QList<BaseLmmElement *> elements;
	QTimer timer;
	QUrl sourceUrl;

	virtual int startElement(BaseLmmElement *el);
	virtual int startPlayer() = 0;
	virtual int stopPlayer() = 0;
	virtual int elementStarted(BaseLmmElement *el);
	int processBuffer(const RawBuffer &);
	virtual void timeoutHandler() {}

	friend class LmmSettingHandler;
	virtual const QList<LmmThread *> getThreads() = 0;
	const QList<BaseLmmElement *> getElements() { return elements; }
};

#endif // BASEPLAYER_H
