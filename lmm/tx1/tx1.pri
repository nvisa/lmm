HEADERS += \
    tx1/tx1videoencoder.h \
    tx1/nvidia/Queue.h \
    tx1/nvidia/StreamConsumer.h \
    tx1/nvidia/TRTStreamConsumer.h \
    tx1/nvidia/VideoEncoder.h \
    tx1/nvidia/VideoEncodeStreamConsumer.h \
    tx1/tx1videodecoder.h \
    tx1/tx1jpegencoder.h

SOURCES += \
    tx1/tx1videoencoder.cpp \
    tx1/nvidia/NvApplicationProfiler.cpp \
    tx1/nvidia/NvBuffer.cpp \
    tx1/nvidia/NvEglRenderer.cpp \
    tx1/nvidia/NvElement.cpp \
    tx1/nvidia/NvElementProfiler.cpp \
    tx1/nvidia/NvJpegDecoder.cpp \
    tx1/nvidia/NvJpegEncoder.cpp \
    tx1/nvidia/NvLogging.cpp \
    tx1/nvidia/NvUtils.cpp \
    tx1/nvidia/NvV4l2Element.cpp \
    tx1/nvidia/NvV4l2ElementPlane.cpp \
    tx1/nvidia/NvVideoConverter.cpp \
    tx1/nvidia/NvVideoDecoder.cpp \
    tx1/nvidia/NvVideoEncoder.cpp \
    tx1/nvidia/VideoEncoder.cpp \
    tx1/tx1videodecoder.cpp \
    tx1/tx1jpegencoder.cpp

UNUSED_SOURCES = \
    tx1/nvidia/NvDrmRenderer.cpp \
    tx1/nvidia/StreamConsumer.cpp \
    tx1/nvidia/VideoEncodeStreamConsumer.cpp \
    tx1/nvidia/TRTStreamConsumer.cpp \

INCLUDEPATH += tx1/nvidia
INCLUDEPATH += tx1/nvidia/include tx1/nvidia/include/libjpeg-8b
