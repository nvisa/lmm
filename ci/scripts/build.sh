#!/bin/sh
set -x
warning(){
    echo "-n: Bu parametre zorunludur. Uygulama ismini içermelidir. Örneğin ecl, lmm vb. gibi."
    echo "-d: Bu parametre zorunludur. Build edilecek cihaz bilgisini içermelidir.
    Kullanılması gereken parametreler;
    * Nvidia Tx1 cihazlar için 'arm64-tx1'
    * Nvidia Jetson Tk1 için 'arm32-jtk1'
    * x86_64(desktop) Ubuntu 16.04 için 'amd64-16.04'
    * x86_64(desktop) Ubuntu 18.04 için 'amd64-18.04' olmak zorundadır."
    echo "Kullanım notları:
    * Build klasör yollarını ASLA değiştirmeyin. Build işlemi sonucu oluşan dosyaları
    \"artifact\" olarak aldığımız için \"artifact\" dosya yolu bu dizinlere bağlıdır.
    * Dosya içerisinde bağımlılıkları indirme ve build işlemleri dışında herhangi birşeyi 
    ASLA değiştirmeyin. Aksi durumda CI/CD işlemleri hatalı sonuçlanacaktır.
    (Örneğin; Projenizin Jetson TK1 desteği olmasını istemiyorsanız bile \"jtk1build\" fonksiyonunu silmeyiniz.)"    
}

if [ $# -eq 0 ]
  then
    warning
    exit
fi

NAME=""
DEVICE=""

while getopts n:d: option
do
case "${option}"
in
    n) NAME=${OPTARG}
        ;;
    d) DEVICE=${OPTARG}
        ;;

esac
done

choose_device() {
    if [ $DEVICE = "arm64-tx1" ]
        then
            tx1build
    elif [ $DEVICE = "arm32-jtk1" ]
        then
            jtk1build
    elif [ $DEVICE = "amd64-16.04" ]
        then
            amd64_16
    elif [ $DEVICE = "amd64-18.04" ]
        then
            amd64_18
    else warning
    
    fi
}

tx1build() {
    echo "tx1 build script"
    apt update
    apt install -y qt5-default make libqt5serialport5-dev libserial-dev g++ wget git libssl1.0.0 
    apt install -y libgtk2.0-dev libavcodec-dev libavformat-dev libavutil-dev curl openssh-client
    apt install -y libavfilter-dev libgnutls-dev libjson0-dev libgcrypt11-dev ffmpeg 
    apt install -y libx264-dev libsrtp0-dev liblzma-dev
    apt install -y libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad 
    apt install -y gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-doc gstreamer1.0-tools 
    wget https://gitlab.com/ozogulf/ci-files/raw/master/tx1/libyuv_TX1.tar.gz
    tar xf libyuv_TX1.tar.gz -C /usr/local
    cp lmm/build_config.pri.sample lmm/build_config.pri 
    mkdir -p build-debug/lmm && cd build-debug/lmm
    qmake ../../lmm CONFIG+=debug CONFIG+=tx1
    make -j4 install
    cd ../../
    mkdir -p build-release/lmm && cd build-release/lmm
    qmake ../../lmm CONFIG+=tx1
    make -j4 install
    cd ../../
    tar cvf $NAME-${CI_COMMIT_REF_NAME}-0.${CI_COMMIT_SHORT_SHA}-debug-${CI_JOB_NAME}.tar.gz build-debug/
    tar cvf $NAME-${CI_COMMIT_REF_NAME}-0.${CI_COMMIT_SHORT_SHA}-release-${CI_JOB_NAME}.tar.gz build-release/
    curl --request GET --header 'PRIVATE-TOKEN: -X1wjQnHacSky81ZHEYa' 'https://gitlab.com/api/v4/projects/10838582/repository/files/ciuser.pem/raw?ref=master' >> ciuser.pem
    chmod 400 ciuser.pem
    scp -i ciuser.pem -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $NAME-${CI_COMMIT_REF_NAME}-${CI_JOB_NAME}-debug-0.${CI_COMMIT_SHORT_SHA}.tar.gz ciuser@storage.sparsetechnology.com:/var/www/html/ci/tar-packages/
    scp -i ciuser.pem -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $NAME-${CI_COMMIT_REF_NAME}-${CI_JOB_NAME}-release-0.${CI_COMMIT_SHORT_SHA}.tar.gz ciuser@storage.sparsetechnology.com:/var/www/html/ci/tar-packages/
}

jtk1build(){
    echo "tk1 build script"
    apt update
    apt install -y qt5-default make libqt5serialport5-dev libserial-dev g++ wget git 
    apt install -y libgtk2.0-dev libavcodec-dev libavformat-dev libavutil-dev
    apt install -y libavfilter-dev libgnutls-dev libjson0-dev libgcrypt11-dev
    apt install -y libx264-dev libsrtp0-dev liblzma-dev
    apt install -y libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad 
    apt install -y gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-doc gstreamer1.0-tools 
    cp lmm/build_config.pri.sample lmm/build_config.pri 
    mkdir -p build-debug/lmm && cd build-debug/lmm
    qmake ../../lmm CONFIG+=debug
    make -j4 install
    cd ../../
    mkdir -p build-release/lmm && cd build-release/lmm
    qmake ../../lmm
    make -j4 install
    cd ../../
    tar cvf $NAME-${CI_COMMIT_REF_NAME}-0.${CI_COMMIT_SHORT_SHA}-debug-${CI_JOB_NAME}.tar.gz build-debug/
    tar cvf $NAME-${CI_COMMIT_REF_NAME}-0.${CI_COMMIT_SHORT_SHA}-release-${CI_JOB_NAME}.tar.gz build-release/
    curl --request GET --header 'PRIVATE-TOKEN: -X1wjQnHacSky81ZHEYa' 'https://gitlab.com/api/v4/projects/10838582/repository/files/ciuser.pem/raw?ref=master' >> ciuser.pem
    chmod 400 ciuser.pem
    scp -i ciuser.pem -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $NAME-${CI_COMMIT_REF_NAME}-${CI_JOB_NAME}-debug-0.${CI_COMMIT_SHORT_SHA}.tar.gz ciuser@storage.sparsetechnology.com:/var/www/html/ci/tar-packages/
    scp -i ciuser.pem -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $NAME-${CI_COMMIT_REF_NAME}-${CI_JOB_NAME}-release-0.${CI_COMMIT_SHORT_SHA}.tar.gz ciuser@storage.sparsetechnology.com:/var/www/html/ci/tar-packages/
}

amd64_16(){
    echo "16.04 build script"
    apt update
    apt install -y qt5-default make libqt5serialport5-dev libserial-dev g++ wget git 
    apt install -y libgtk2.0-dev libavcodec-dev libavformat-dev libavutil-dev curl openssh-client
    apt install -y libavfilter-dev libgnutls-dev libjson0-dev libgcrypt11-dev ffmpeg 
    apt install -y libx264-dev libsrtp0-dev liblzma-dev 
    apt install -y libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad
    apt install -y gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-doc gstreamer1.0-tools libxv-dev
    wget https://gitlab.com/ozogulf/ci-files/raw/master/x86_64/libyuv_16_04.tar.gz
    tar xf libyuv_16_04.tar.gz -C /
    cp lmm/build_config.pri.sample lmm/build_config.pri 
    mkdir -p build-debug/lmm && cd build-debug/lmm
    qmake ../../lmm CONFIG+=debug
    make -j4 install
    cd ../../
    mkdir -p build-release/lmm && cd build-release/lmm
    qmake ../../lmm
    make -j4 install
    cd ../../
    tar cvf $NAME-${CI_COMMIT_REF_NAME}-0.${CI_COMMIT_SHORT_SHA}-debug-${CI_JOB_NAME}.tar.gz build-debug/
    tar cvf $NAME-${CI_COMMIT_REF_NAME}-0.${CI_COMMIT_SHORT_SHA}-release-${CI_JOB_NAME}.tar.gz build-release/
    curl --request GET --header 'PRIVATE-TOKEN: -X1wjQnHacSky81ZHEYa' 'https://gitlab.com/api/v4/projects/10838582/repository/files/ciuser.pem/raw?ref=master' >> ciuser.pem
    chmod 400 ciuser.pem
    scp -i ciuser.pem -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $NAME-${CI_COMMIT_REF_NAME}-${CI_JOB_NAME}-debug-0.${CI_COMMIT_SHORT_SHA}.tar.gz ciuser@storage.sparsetechnology.com:/var/www/html/ci/tar-packages/
    scp -i ciuser.pem -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $NAME-${CI_COMMIT_REF_NAME}-${CI_JOB_NAME}-release-0.${CI_COMMIT_SHORT_SHA}.tar.gz ciuser@storage.sparsetechnology.com:/var/www/html/ci/tar-packages/
}

amd64_18(){
    echo "18.04 build script"
    apt update
    apt install -y qt5-default make libqt5serialport5-dev libserial-dev g++ wget git
    apt install -y libgtk2.0-dev libavcodec-dev libavformat-dev libavutil-dev curl openssh-client
    apt install -y libavfilter-dev libgnutls-dev libjson0-dev libgcrypt11-dev ffmpeg 
    apt install -y libx264-dev libsrtp0-dev liblzma-dev
    apt install -y libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad 
    apt install -y gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-doc gstreamer1.0-tools libxv-dev
    wget https://gitlab.com/ozogulf/ci-files/raw/master/x86_64/libyuv_16_04.tar.gz
    tar xf libyuv_16_04.tar.gz -C /
    cp lmm/build_config.pri.sample lmm/build_config.pri
    mkdir -p build-debug/lmm && cd build-debug/lmm
    qmake ../../lmm CONFIG+=debug
    make -j4 install
    cd ../../
    mkdir -p build-release/lmm && cd build-release/lmm
    qmake ../../lmm
    make -j4 install
    cd ../../
    tar cvf $NAME-${CI_COMMIT_REF_NAME}-0.${CI_COMMIT_SHORT_SHA}-debug-${CI_JOB_NAME}.tar.gz build-debug/
    tar cvf $NAME-${CI_COMMIT_REF_NAME}-0.${CI_COMMIT_SHORT_SHA}-release-${CI_JOB_NAME}.tar.gz build-release/
    curl --request GET --header 'PRIVATE-TOKEN: -X1wjQnHacSky81ZHEYa' 'https://gitlab.com/api/v4/projects/10838582/repository/files/ciuser.pem/raw?ref=master' >> ciuser.pem
    chmod 400 ciuser.pem
    scp -i ciuser.pem -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $NAME-${CI_COMMIT_REF_NAME}-${CI_JOB_NAME}-debug-0.${CI_COMMIT_SHORT_SHA}.tar.gz ciuser@storage.sparsetechnology.com:/var/www/html/ci/tar-packages/
    scp -i ciuser.pem -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $NAME-${CI_COMMIT_REF_NAME}-${CI_JOB_NAME}-release-0.${CI_COMMIT_SHORT_SHA}.tar.gz ciuser@storage.sparsetechnology.com:/var/www/html/ci/tar-packages/
}

choose_device
