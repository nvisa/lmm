#include "roi.h"

#include <lmm/baselmmelement.h>
#include <lmm/debug.h>
#include <QDebug>
#include <QImage>
#include <QFile>
#include <QLabel>
#include <QImage>

#include <errno.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <cv.h>
#include <highgui.h>

using namespace cv;


ROI::ROI(QObject *parent) :
    BaseLmmElement(parent)
{
    roiState = false;
    filename = "roi.png";

}

void ROI::setRoiState(bool state)
{
    roiState = state;
}


int ROI::processBuffer(const RawBuffer &buf)
{
    lock.lock();
    if(!roiState) {
        lock.unlock();
        return newOutputBuffer(0, buf);
    }
    int err = saveImage(buf);
    if(!err) {
        f->close();
        mDebug("Roi image saved");
        roiState = false;
    }

    lock.unlock();
    return newOutputBuffer(0, buf);
}

int ROI::start()
{
    if(!roiState)
        return BaseLmmElement::start();

    f = new QFile(filename);
    if(!f->open(QIODevice::WriteOnly)) {
        mDebug("error opening output file %s ", qPrintable(f->fileName()));
        return -EINVAL;
    }

    return BaseLmmElement::start();

}

int ROI::saveImage(const RawBuffer &buf)
{
    if(!f->isOpen()) {
        if(!f->open(QIODevice::WriteOnly)) {
            mDebug("error opening output file %s", qPrintable(f->fileName()));
            return -EINVAL;
        }
    }
    unsigned char *buf1 = (uchar *)buf.constData();
    int height = buf.constPars()->videoHeight;
    int width = buf.constPars()->videoWidth;
#if 0
    // gray
    Mat frame(height, width, CV_8U, buf1);
    imwrite("gst1.png", frame);
#endif
#if 1
    Mat frame(height, width, CV_8U, buf1);
    Mat frameU(height / 2, width, CV_8U, buf1+width*height);

    Mat frameYUV(height, width, CV_8UC3);
    Mat frameRGB(height, width, CV_8UC3);
    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            frameYUV.data[3*(j+i*width)]   = frame.data[j+i*width];
            frameYUV.data[3*(j+i*width)+1] = frameU.data[2*(j/2)+(i/2)*width];
            frameYUV.data[3*(j+i*width)+2] = frameU.data[2*(j/2)+(i/2)*width+1];
        }
    }
    cvtColor(frameYUV, frameRGB, COLOR_YUV2RGB);
    if(!imwrite("opencv_image.png", frameRGB))
        return -EINVAL;
#endif
    return 0;
}
