#ifndef BASEPLAYER_H
#define BASEPLAYER_H

#include <lmm/baselmmelement.h>
#include <lmm/lmmcommon.h>
#include <lmm/lmmthread.h>

#include <QUrl>
#include <QTimer>

class RemoteConsole;

template <class T>
class OpThread : public LmmThread
{
public:
	typedef int (T::*threadOp)();
	OpThread(T *parent, threadOp op, QString name)
		: LmmThread(name)
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
	virtual int setFilename(QString filename);
signals:
	
public slots:
	virtual void timeout();
protected:
	QList<BaseLmmElement *> elements;
	QTimer timer;
	QUrl sourceUrl;
	RemoteConsole *manCons;

	virtual int startElement(BaseLmmElement *el);
	virtual int startPlayer() = 0;
	virtual int stopPlayer() = 0;
	virtual int elementStarted(BaseLmmElement *el);
	virtual RemoteConsole * createManagementConsole();

	friend class LmmSettingHandler;
	virtual const QList<LmmThread *> getThreads() = 0;
	const QList<BaseLmmElement *> getElements() { return elements; }
};

#endif // BASEPLAYER_H
