TARGET = lmm
TEMPLATE = lib
CONFIG += staticlib

QT += network

greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent

include (build_config.pri)

SOURCES += \
    filesource.cpp \
    rawbuffer.cpp \
    circularbuffer.cpp \
    baselmmelement.cpp \
    baselmmdecoder.cpp \
    baselmmoutput.cpp \
    streamtime.cpp \
    fileoutput.cpp \
    lmmcommon.cpp \
    v4l2input.cpp \
    fboutput.cpp \
    debugserver.cpp \
    debugclient.cpp \
    udpoutput.cpp \
    udpinput.cpp \
    lmmthread.cpp \
    tools/unittimestat.cpp \
    tools/videoutils.cpp \
    rtsp/basertspserver.cpp \
    debug.cpp \
    hardwareoperations.cpp \
    v4l2output.cpp \
    buffersyncer.cpp \
    lmmbufferpool.cpp \
    udpsource.cpp \
    baselmmparser.cpp \
    h264parser.cpp \
    baseplayer.cpp \
    tools/cpuload.cpp \
    tools/rawnetworksocket.cpp \
    baselmmpipeline.cpp \
    pipeline/basepipeelement.cpp \
    pipeline/functionpipeelement.cpp \
    bufferqueue.cpp \
    textoverlay.cpp \
    tools/errorinjector.cpp \
    tools/threadsafelist.cpp \
    pipeline/pipelinedebugger.cpp \
    pipeline/pipelinemanager.cpp \
    rtp/rtptransmitter.cpp \
    players/basestreamer.cpp \
    genericvideotestsource.cpp \
    interfaces/streamcontrolelementinterface.cpp \
    tools/tokenbucket.cpp \
    rtp/rtpreceiver.cpp \
    rtsp/rtspclient.cpp \
    jobdistributorelement.cpp \
    interfaces/videostreaminterface.cpp \
    multibuffersource.cpp \
    metrics.cpp

HEADERS  += \
    filesource.h \
    rawbuffer.h \
    circularbuffer.h \
    baselmmelement.h \
    baselmmdecoder.h \
    baselmmoutput.h \
    streamtime.h \
    fileoutput.h \
    lmmcommon.h \
    v4l2input.h \
    fboutput.h \
    debugserver.h \
    debugclient.h \
    udpoutput.h \
    udpinput.h \
    lmmthread.h \
    tools/unittimestat.h \
    tools/videoutils.h \
    rtsp/basertspserver.h \
    debug.h \
    hardwareoperations.h \
    platform_info.h \
    v4l2output.h \
    buffersyncer.h \
    lmmbufferpool.h \
    udpsource.h \
    baselmmparser.h \
    h264parser.h \
    baseplayer.h \
    tools/cpuload.h \
    tools/rawnetworksocket.h \
    baselmmpipeline.h \
    pipeline/basepipeelement.h \
    pipeline/functionpipeelement.h \
    bufferqueue.h \
    textoverlay.h \
    tools/errorinjector.h \
    tools/threadsafelist.h \
    pipeline/pipelinedebugger.h \
    pipeline/pipelinemanager.h \
    rtp/rtptransmitter.h \
    players/basestreamer.h \
    genericvideotestsource.h \
    interfaces/streamcontrolelementinterface.h \
    interfaces/imagesnapshotinterface.h \
    tools/tokenbucket.h \
    interfaces/motiondetectioninterface.h \
    rtp/rtpreceiver.h \
    rtsp/rtspclient.h \
    version.h \
    jobdistributorelement.h \
    interfaces/videostreaminterface.h \
    multibuffersource.h \
    metrics.h

lessThan(QT_VERSION, 4.7) {
    SOURCES += compat/qelapsedtimer.cpp compat/qelapsedtimer_unix.cpp
    HEADERS += compat/qelapsedtimer.h compat/QElapsedTimer
    INCLUDEPATH += compat
}

grpc {
	DEFINES += HAVE_GRPC
	include(pipeline/proto/proto.pri)
}

metrics {
	include($$PWD/../../../yca/MetricsLib/metrics.pri)
	INCLUDEPATH += $$PWD/../../../yca/MetricsLib
}

widgets {
    SOURCES += qtvideooutput.cpp
    HEADERS += qtvideooutput.h
}

x264 {
    SOURCES += x264encoder.cpp
    HEADERS += x264encoder.h
}

x11 {
    HEADERS += x11videooutput.h
    SOURCES += x11videooutput.cpp

    !staticlib {
	LIBS += -lX11 -lXv
    }
}
alsa {
    HEADERS += \
        alsa/alsa.h \
		alsa/alsaoutput.h \
		alsa/alsainput.h \

    SOURCES += \
        alsa/alsa.cpp \
		alsa/alsaoutput.cpp \
		alsa/alsainput.cpp \

    DEFINES += CONFIG_ALSA

    !staticlib {
	LIBS += -lasound
    }
}

mad {
    SOURCES += maddecoder.cpp \
        mp3demux.cpp \

    HEADERS += maddecoder.h \
        mp3demux.h \
    DEFINES += CONFIG_MAD
}

ffmpeg {
    HEADERS += \
        ffmpeg/avidemux.h \
        ffmpeg/mpegtsdemux.h \
        ffmpeg/baselmmdemux.h \
        ffmpeg/baselmmmux.h \
        ffmpeg/mp4mux.h \
        ffmpeg/avimux.h \
        ffmpeg/ffcompat.h \
        ffmpeg/ffmpegbuffer.h \
        ffmpeg/ffmpegcolorspace.h \
        ffmpeg/ffmpegcontexter.H \
        ffmpeg/ffmpegdecoder.h \
        videoscaler.h \

    SOURCES += \
        ffmpeg/baselmmdemux.cpp \
        ffmpeg/mpegtsdemux.cpp \
        ffmpeg/avidemux.cpp \
        ffmpeg/baselmmmux.cpp \
        ffmpeg/mp4mux.cpp \
        ffmpeg/avimux.cpp \
        ffmpeg/ffmpegbuffer.cpp \
        ffmpeg/ffmpegcolorspace.cpp \
        ffmpeg/ffmpegcontexter.cpp \
        ffmpeg/ffmpegdecoder.cpp \
        videoscaler.cpp \

            dm6446 {
                    SOURCES += \
                            ffmpeg/mpegtsmux.cpp \
                            ffmpeg/audioencoder.cpp \

                    HEADERS += \
                            ffmpeg/mpegtsmux.h \
                            ffmpeg/audioencoder.h \
            }

    ffmpeg_rtp {
        HEADERS += \
            ffmpeg/rtph264mux.h \
            ffmpeg/rtpmjpegmux.h \
            ffmpeg/rtpmux.h \

        SOURCES += \
            ffmpeg/rtph264mux.cpp \
            ffmpeg/rtpmjpegmux.cpp \
            ffmpeg/rtpmux.cpp \

    }
    DEFINES += CONFIG_FFMPEG

    #when building non-static libraries following libraries needed so that lmm can be used in plugins
    !staticlib {
        LIBS += -L$$/usr/lib \
            -lavdevice \
            -lavformat \
            -lavfilter \
            -lavcodec \
            -lswresample \
            -lswscale \
            -lavutil \
            -lfdk-aac \
            -lx264 \
            -lpostproc \
    }
}

ffmpeg_extra {
    SOURCES += \
        ffmpeg/ffmpegextra.cpp \
        ffmpeg/ffmpeg_extra.c
    HEADERS += \
        ffmpeg/ffmpegextra.h
    INCLUDEPATH += "/home/amenmd/myfs/source-codes/oss/FFmpeg"
    DEFINES += CONFIG_FFMPEG_EXTRA
}

dmai { include(dmai/dmai.pri) }

dvb {
    SOURCES += \
        dvb/tsdemux.cpp \
        dvb/dvbutils.cpp \

    HEADERS  += \
        dvb/tsdemux.h \
        dvb/dvbutils.h \

    QT += sql
}

neon {

    SOURCES += neon/neonvideoscaler.cpp \
	neon/neoncolorspace.cpp \
	neon/libyuv.c \

    HEADERS += neon/neonvideoscaler.h \
	neon/neoncolorspace.h \
	neon/libyuv.h \
}

zynq {

    SOURCES += zynq/zynqvideoinput.cpp \

    HEADERS += zynq/zynqvideoinput.h \

}

dm365 { include(dm365/dm365.pri) }

dm6446 {
    include(dm6446/tipaths.pri)
    DEFINES += CONFIG_DM6446
    SOURCES += \
        dm6446/dm6446fboutput.cpp \
        dm6446/platformcommondm6446.cpp \

    HEADERS  += \
        dm6446/dm6446fboutput.h \
        dm6446/platformcommondm6446.h \

    xdc.files += dm6446/tipaths.pri
    xdc.files += dm6446/dm6446.pri
    xdc.files += dm6446/config.bld
    xdc.files += dm6446/dm6446.cfg
    xdc.path = $$INSTALL_PREFIX/usr/local/include/lmm/dm6446

    OTHER_FILES += \
        dm6446/dm6446.pri \
        dm6446/dm6446.cfg \
        dm6446/config.bld \

}

omx {
	DEFINES += CONFIG_OMX
	SOURCES += \
		omx/baseomxelement.cpp \
		omx/omxdecoder.cpp \
		omx/omxdisplayoutput.cpp \
		omx/omxscaler.cpp \

	HEADERS += \
		omx/baseomxelement.h \
		omx/omxdecoder.h \
		omx/omxdisplayoutput.h \
		omx/omxscaler.h \
}

gstreamer {
	DEFINES += CONFIG_GSTREAMER
        GST_FLAGS = $$system(pkg-config gstreamer-1.0 --cflags-only-I | sed 's/-I//g')
        INCLUDEPATH += $$GST_FLAGS

	SOURCES += \
                gstreamer/lmmgstpipeline.cpp \
                gstreamer/basegstcaps.cpp \

	HEADERS += \
		gstreamer/lmmgstpipeline.h \
                gstreamer/basegstcaps.h \

}

dm8168 {
	include(dm8168/tipaths.pri)
	DEFINES += CONFIG_DM8168

	SOURCES += \
		dm8168/dm8168videocontroller.cpp \
		dm8168/platformcommondm8168.cpp \

	HEADERS += \
		dm8168/dm8168videocontroller.h \
		dm8168/platformcommondm8168.h \

	xdc.files += dm8168/tipaths.pri
	xdc.files += dm8168/dm8168.pri
        xdc.path = $$INSTALL_PREFIX/usr/local/include/lmm/dm8168

	OTHER_FILES += \
		dm365/dm8168.pri \
}

srtp {
    HEADERS += \
        rtp/srtptransmitter.h \
        rtp/srtpreceiver.h
    SOURCES += \
        rtp/srtptransmitter.cpp \
        rtp/srtpreceiver.cpp
}

tx1 { include(tx1/tx1.pri) }

INCLUDEPATH += ..

headers.files = lmm.pri
headers.path = $$INSTALL_PREFIX/usr/local/include/lmm

target.path = $$INSTALL_PREFIX/usr/local/lib

INSTALLS += target headers

#Following for installs each module into its own subdirectory
#without this all files will be installed into 1 folder
for(h, HEADERS) {
    file=$$join(h, "", "$$INSTALL_PREFIX/usr/local/include/lmm/")
    dir=$$dirname(file)
    base=$$basename(dir)
    !contains(paths, $$dir) {
        paths+=$$dir
        eval($${base}.path = $$dir)
        INSTALLS += $$base
    }
    eval($${base}.files += $$h)
}

OTHER_FILES += \
    build_config.pri \
    lmm.pri \
    dm8168/tipaths.pri \
    dm8168/dm8168.pri

#Add make targets for checking version info
VersionCheck.commands = @$$PWD/checkversion.sh $$PWD
QMAKE_EXTRA_TARGETS += VersionCheck
PRE_TARGETDEPS += VersionCheck

DEPENDPATH += $$INCLUDEPATH
