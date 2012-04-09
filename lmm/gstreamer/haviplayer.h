#ifndef HAVIPLAYER_H
#define HAVIPLAYER_H

#include "abstractgstreamerinterface.h"

#include <QStringList>
#include <QString>
#include <QSemaphore>

class hAviPlayer : public AbstractGstreamerInterface
{
public:
	hAviPlayer(int winId, QObject *parent = 0);
	hAviPlayer(QObject *parent = 0);

	void setMediaSource(QString source) { vFileName = source; }
	QString getMediaSource() { return vFileName; }
	void setSubtitleFileName(QString str);

	/* Snapshot stuff */
	void takeSnapShots(int intervalSecs, int count);
	int getSnapShotSize() { return snapShotSize; }
	int getSnapShotWidth() { return snapShotWidth; }
	int getSnapShotHeight() { return snapShotHeight; }
	int getSnapShotInterval() { return snapShotIntervalSecs; }
	void setSnapShotInterval(int secs) { snapShotIntervalSecs = secs; }
	unsigned int getSnapShotFourcc() { return snapShotFourcc; }
	void * getSnapShot(int index) { return index < snapshots.size() ? snapshots[index] : NULL; }
	void newApplicationMessageReceived();

protected:
	int createElementsList();
	int createVideoElements();
	int createAudioElements();
	void pipelineCreated();
	bool newPadBuffer(GstBuffer *, GstPipeElement *el);
private:
	QString vFileName;
	QString subtitleFileName;
	int widgetWinId;

	/* Snapshot related variables */
	QList<char *> snapshots;
	int snapShotSize;
	int snapShotWidth;
	int snapShotHeight;
	unsigned int snapShotFourcc;
	int snapShotIntervalSecs;
	QSemaphore ssSync;
};

#endif // HAVIPLAYER_H
