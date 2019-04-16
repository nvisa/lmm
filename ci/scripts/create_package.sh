set -x
warning(){
    echo "-n: Bu parametre zorunludur. Uygulama ismini içermelidir. Örneğin ecl, lmm vb. gibi."
    echo "-b: Bu parametre zorunludur. Branch ismini içermelidir."
    echo "-j: Bu parametre zorunludur. İlgili Job ismini içermelidir."
    echo "-c: Bu parametre zorunludur. Commit id içermelidir."   
}

if [ $# -eq 0 ]
  then
    warning
    exit
fi

NAME=""
BRANCH=""
JNAME=""
COMMITID=""

while getopts n:b:j:c: option
do
case "${option}"
in
    n) NAME=${OPTARG}
        ;;
    b) BRANCH=${OPTARG}
        ;;
    j) JNAME=${OPTARG}
        ;;
    c) COMMITID=${OPTARG}
        ;;

esac
done

package(){
    apt update
    ls
    apt-get install -y wget libssl1.0.0
    wget https://gitlab.com/ozogulf/ci-files/raw/master/paketleme/DEBIAN_FILES.tar.gz
    tar -xvf DEBIAN_FILES.tar.gz
    ls
    wget https://gitlab.com/ozogulf/ci-files/raw/master/paketleme/prepare-deb.sh
    chmod +x prepare-deb.sh
    ./prepare-deb.sh -a $NAME-$BRANCH-$JNAME-release -v $COMMITID
    ls
    cp -r ./build-release/usr $NAME-$BRANCH-$JNAME-release_${COMMITID}
    cp lmm/build_config.pri.sample $NAME-$BRANCH-$JNAME-release_${COMMITID} 
    mkdir -p debPackage-release
    cp -r $NAME-$BRANCH-$JNAME-release_${COMMITID}/ debPackage-release/
    cd debPackage-release
    dpkg-deb -b $NAME-$BRANCH-$JNAME-release_${COMMITID}
    ls
    mv $NAME-$BRANCH-$JNAME-release_${COMMITID}.deb $NAME:$BRANCH:$JNAME:release_${COMMITID}.deb
    cd ../
    ./prepare-deb.sh -a $NAME-$BRANCH-$JNAME-debug -v ${COMMITID}
    cp -r ./build-debug/usr $NAME-$BRANCH-$JNAME-debug_${COMMITID}
    cp lmm/build_config.pri.sample $NAME-$BRANCH-$JNAME-debug_${COMMITID} 
    mkdir -p debPackage-debug
    cp -r $NAME-$BRANCH-$JNAME-debug_${COMMITID}/ debPackage-debug/
    cd debPackage-debug
    ls
    dpkg-deb -b $NAME-$BRANCH-$JNAME-debug_${COMMITID}
    mv $NAME-$BRANCH-$JNAME-debug_${COMMITID}.deb $NAME:$BRANCH:$JNAME:debug_${COMMITID}.deb
}

package
