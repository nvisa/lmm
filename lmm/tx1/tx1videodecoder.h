#ifndef TX1VIDEODECODER_H
#define TX1VIDEODECODER_H

#include <lmm/baselmmdecoder.h>

class NvPriv;

class TX1VideoDecoder : public BaseLmmDecoder
{
    Q_OBJECT
public:
    TX1VideoDecoder(QObject *parent = 0);

    void decodedBufferReady();
    void aboutToDeleteBuffer(const RawBufferParameters *params);

protected:
    virtual int decode(RawBuffer buf);
    virtual int startDecoding();
    virtual int stopDecoding();

    NvPriv *priv;
};

#endif // TX1VIDEODECODER_H
