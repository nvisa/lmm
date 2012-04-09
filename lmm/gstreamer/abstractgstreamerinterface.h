/*
 * Copyright (C) 2010 Yusuf Caglar Akyuz
 *
 * Her hakki Bilkon Ltd. Sti.'ye  aittir.
 */

#ifndef ABSTRACTGSTREAMERINTERFACE_H
#define ABSTRACTGSTREAMERINTERFACE_H

#include <QObject>

#include <gst/gst.h>

#include <QMap>
#include <QVariant>
#include <QSemaphore>
#include <QStringList>

void demux_pad_removed(GstElement *, GstPad *, gpointer);
void demux_pad_added(GstElement *, GstPad *pad, gpointer data);
gboolean handleBusMessage(GstBus *, GstMessage *msg, gpointer data);
gboolean padWatcher(GstPad *pad, GstMiniObject *, gpointer user_data);

class AbstractGstreamerInterface;

class GstPipeElement : public QObject
{
    Q_OBJECT
public:
    enum padType {
        PAD_ALWAYS,
        PAD_SOMETIMES,
        PAD_REQUEST,
    };

    explicit GstPipeElement(QString gstElement, QString nodeName, QObject *parent = 0);

    void addParameter(QString key, bool v) { boolValues.insert(key, v); }
    void addParameter(QString key, int v) { intValues.insert(key, v); }
    void addParameter(QString key, unsigned long long v) { longLongValues.insert(key, v); }
    void addParameter(QString key, const char *v) { strValues.insert(key, v); }
    void addParameter(QString key, QString v) { strValues.insert(key, qPrintable(v)); }
    void addParameter(QString key, GValue *v)
    {
        GValue *vc = new GValue();
        gst_value_init_and_copy(vc, v);
        gValues.insert(key, vc);
    }

    QString gstElementName() { return elementName; }
    QString nodeName() { return name; }
    void incrementBufferCount() { bufferCount++; }

    void addSourceLink(QString nodeName, QString srcPadName, padType type1,
                       QString destPadName, padType type2)
    {
        srcLinkElements << nodeName;
        srcLinkElementPads << srcPadName;
        elementPads << destPadName;
        srcPadTypes << type1;
        destPadTypes << type2;
    }
    void modifySourceLink(int pos, QString srcPadName, padType type1,
                       QString destPadName, padType type2)
    {
        srcLinkElementPads[pos] = srcPadName;
        elementPads[pos] = destPadName;
        srcPadTypes[pos] = type1;
        destPadTypes[pos] = type2;
    }
    QStringList getSourceElementNames() { return srcLinkElements; }
    QStringList getSourceElementPads() { return srcLinkElementPads; }
    QStringList getElementPads() { return elementPads; }

    bool isCapsFilter();
    void setNoLink(bool v) { doNotLink = v; }
    bool linkThisElement() { return !doNotLink; }
    void watchThisElement(bool v) { watchElement = v; }
    AbstractGstreamerInterface * parent() { return parentInterface; }
    void setGstElement(GstElement *el) { gstElement = el; }
    GstElement * getGstElement() { return gstElement; }
private:

    friend class AbstractGstreamerInterface;
    AbstractGstreamerInterface *parentInterface;
    QString elementName;
    QString name;
    QMap<QString, bool> boolValues;
    QMap<QString, int> intValues;
    QMap<QString, unsigned long long> longLongValues;
    QMap<QString, QString> strValues;
    QMap<QString, GValue *> gValues;

    int lastBufferCount;
    int bufferCount;

    QStringList srcLinkElements;
    QStringList srcLinkElementPads;
    QStringList elementPads;
    QList<int> srcPadTypes;
    QList<int> destPadTypes;

    bool doNotLink;
    bool watchElement;

    GstElement *gstElement;
};

class AbstractGstreamerInterface : public QObject
{
    Q_OBJECT
public:
    explicit AbstractGstreamerInterface(QObject *parent = 0);
    ~AbstractGstreamerInterface();

    virtual int checkPipelineStatus();
    int elementIndex(QString nodeName);

    virtual int startPlayback();
    virtual int stopPlayback();
    virtual int pausePlayback();

    virtual void setMediaSource(QString source) = 0;
    virtual QString getMediaSource() = 0;
    virtual void setPlaybackParameter(QString key, QVariant value) { parameters.insert(key, value); }
    bool isRunning() { return pipeline ? pipelineRunning : false; }
    bool isPaused() { return pipeline ? pipelinePaused : false; } //TODO: Why is this necessary, isRunning???

    static void link_to_static_sink_pad(GstElement *decoder, GstElement *demux, const char * padName);
    static void link_to_static_sink_pad(GstElement *decoder, GstPad *pad);

    GstElement * getGstPipeElement(QString name);

    virtual unsigned long long getPlaybackPosition();
    virtual unsigned long long getPlaybackDuration();
    virtual void seekToTime(unsigned long long nanoSecs);
    virtual void seek(unsigned long long nanoSecs);
    void setPlaybackSpeed(gdouble rate);
    QString getLastError() { return lastPipelineErrorMsg; }

    static void cleanUp();

protected:
    QString pipelineName;
    GstElement *pipeline;
    QList<GstPipeElement *> pipeElements;
    QMap<QString, GstElement *> gstPipeElements;
    QMap<QString, QVariant> parameters;
    QMap<unsigned int, GstPad *> bufferProbeIDs;
    int elementCount;
    virtual void newPadBuffer(GstElement *, GstPad *) {}
    virtual int createPipeline();
    virtual int destroyPipeline();
    virtual int createElementsList() = 0;
    virtual void pipelineCreated() {}
    virtual bool newPadBuffer(GstBuffer *, GstPipeElement *) { return true; }
    virtual void newApplicationMessageReceived() {}
    virtual int newElementMessageReceived(QString) { return 0; }
    GstPipeElement * addElement(QString elementName, QString nodeName, QString srcElement = "");
    GstElement * getGstElement(QString nodeName);
    GstPipeElement * findElementByNodeName(QString name);
    virtual void setPipelineError(int err, QString msg) { lastPipelineError = err; lastPipelineErrorMsg = msg; }

    int installPadProbes();
    int removePadProbes();

private:
    friend gboolean handleBusMessage(GstBus *, GstMessage *msg, gpointer data);
    friend gboolean padWatcher(GstPad *pad, GstMiniObject *mObj, gpointer user_data);
    void newPadBufferFromGst(GstElement *el, GstPad *pad) { newPadBuffer(el, pad); }
    bool pipelineRunning;
    bool pipelinePaused;
    bool padProbesInstalled;

    int linkElement(GstPipeElement *el, int currentPos, GstCaps *caps, int linkNo = -1);
    int linkElement2(GstPipeElement *el, int currentPos, int linkNo);
    void pipelineStateChanged(GstState state);
    int createElements();
    int setElementParameters(GstPipeElement *el, GstElement *node);
    GstCaps * createCaps(GstPipeElement *el);
    QString findPadToLink(GstElement *node, bool findSinkPads, QString padName);
    GstPipeElement * findPreviousElementToLink(int elementPos, int linkNo);
    int removeFlowPadProbes();
    int installFlowPadProbe(QString);

    QMap<unsigned int, GstPad *> flowBufferProbeIDs;
    void syncWait();
    void sync();
    bool startSyncWait;
    QSemaphore sSync;
    gint64 playbackDuration;
    GstQuery *positionQuery;
    int lastPipelineError;
    QString lastPipelineErrorMsg;
    volatile GstState gstPipelineState;
};

#endif // ABSTRACTGSTREAMERINTERFACE_H
