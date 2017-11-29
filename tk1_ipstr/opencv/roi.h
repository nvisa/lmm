#ifndef ROI_H
#define ROI_H

#include <QFile>

#include <lmm/baselmmelement.h>

class RawBuffer;
class ROI : public BaseLmmElement
{
    Q_OBJECT
public:
    explicit ROI(QObject *parent = 0);
    void setRoiState(bool state);

signals:
protected:
    virtual int processBuffer(const RawBuffer &buf);
    virtual int start();
    int saveImage(const RawBuffer &buf);
public slots:
private:
    QFile *f;
    bool roiState;
    QString filename;
    QMutex lock;
};

#endif // ROI_H
