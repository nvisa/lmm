TARGET = lmm
TEMPLATE = lib
CONFIG += staticlib

QT += network

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
    tools/systeminfo.cpp \
    rtsp/basertspserver.cpp \
    debug.cpp \
    hardwareoperations.cpp \
    v4l2output.cpp \
    buffersyncer.cpp \
    lmmbufferpool.cpp \
	udpsource.cpp \
	baselmmparser.cpp \
	h264parser.cpp \
    rtp/rtppacketizer.cpp \
    baseplayer.cpp \
    tools/remoteconsole.cpp \
    tools/basesettinghandler.cpp \
	tools/lmmsettinghandler.cpp \
	tools/cpuload.cpp \
    players/simplevideoplayer.cpp \
    tools/rawnetworksocket.cpp

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
    textoverlay.h \
    debugserver.h \
    debugclient.h \
    udpoutput.h \
    udpinput.h \
    lmmthread.h \
    tools/unittimestat.h \
    tools/videoutils.h \
    tools/systeminfo.h \
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
    rtp/rtppacketizer.h \
    baseplayer.h \
    tools/remoteconsole.h \
    tools/basesettinghandler.h \
	tools/lmmsettinghandler.h \
	tools/cpuload.h \
	players/simplevideoplayer.h \
    tools/rawnetworksocket.h

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

    SOURCES += \
		ffmpeg/avidemux.cpp \
        ffmpeg/baselmmdemux.cpp \
        ffmpeg/mpegtsdemux.cpp \
        ffmpeg/avidemux.cpp \
        ffmpeg/baselmmmux.cpp \
        ffmpeg/mp4mux.cpp \
        ffmpeg/avimux.cpp \
		ffmpeg/ffmpegbuffer.cpp \
		ffmpeg/ffmpegcolorspace.cpp \

	x86 {
		SOURCES += \
			ffmpeg/ffmpegdecoder.cpp\

		HEADERS += \
			ffmpeg/ffmpegdecoder.h \
	}

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
}

dmai {
    SOURCES += dmai/dmaiencoder.cpp \
        dmai/dmaibuffer.cpp \
        dmai/jpegencoder.cpp \
        dmai/h264encoder.cpp \
		dmai/dmaidecoder.cpp \
		dmai/videotestsource.cpp \

    HEADERS += dmai/dmaiencoder.h \
        dmai/dmaibuffer.h \
        dmai/jpegencoder.h \
        dmai/h264encoder.h \
		dmai/dmaidecoder.h \
		dmai/videotestsource.h \

}

dvb {
    SOURCES += \
        dvb/tsdemux.cpp \
        dvb/dvbutils.cpp \

    HEADERS  += \
        dvb/tsdemux.h \
        dvb/dvbutils.h \

    QT += sql
}

dm365 {
    include(dm365/tipaths.pri)
    DEFINES += CONFIG_DM365
    SOURCES += \
        dm365/dm365camerainput.cpp \
        dm365/dm365videooutput.cpp \
        dm365/platformcommondm365.cpp \
        textoverlay.cpp \


    HEADERS += dm365/dm365camerainput.h \
        dm365/dm365videooutput.h \
        dm365/platformcommondm365.h \
        textoverlay.h \

    xdc.files += dm365/tipaths.pri
    xdc.files += dm365/dm365.pri
    xdc.files += dm365/config.bld
    xdc.files += dm365/dm365.cfg
	xdc.files += dm365/ih264venc.h
    xdc.path = /usr/local/include/lmm/dm365
    CONFIG += arm

    OTHER_FILES += \
        dm365/dm365.pri \
        dm365/dm365.cfg \
        dm365/config.bld
}

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
    xdc.path = /usr/local/include/lmm/dm6446
    CONFIG += arm

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

	SOURCES += \
		gstreamer/lmmgstpipeline.cpp\

	HEADERS += \
		gstreamer/lmmgstpipeline.h \

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
	xdc.path = /usr/local/include/lmm/dm8168
	CONFIG += arm

	OTHER_FILES += \
		dm365/dm8168.pri \
}

armv5te {
    CONFIG += arm
}

x86 {
    DEFINES += TARGET_x86
}

arm {
    DEFINES += TARGET_ARM
}

INCLUDEPATH += ..

headers.files = lmm.pri
headers.path = /usr/local/include/lmm

target.path = /usr/local/lib

INSTALLS += target headers xdc

#Following for installs each module into its own subdirectory
#without this all files will be installed into 1 folder
for(h, HEADERS) {
    file=$$join(h, "", "/usr/local/include/lmm/")
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
