
if [ $HOSTNAME = 'seaquestdaq01.fnal.gov' ] ; then
    source /opt/e1039-share/this-share.sh
    export OFFLINE_MAIN=/opt/e1039-share/inst
    export MY_INSTALL=~/tmp/e1039-core

    export         PATH=$MY_INSTALL/bin:$OFFLINE_MAIN/bin:$PATH
    export        CPATH=$MY_INSTALL/include:$OFFLINE_MAIN/include:$CPATH
    export LIBRARY_PATH=$MY_INSTALL/lib:$OFFLINE_MAIN/lib:$LIBRARY_PATH
    
else
    echo "Using the non-host-specific setting.  This might not work on your environment."
    export  CC=gcc-4.9
    export CXX=g++-4.9
    source /opt/root/root-5.34.36/bin/thisroot.sh
    source /opt/geant4/geant4.10.04.p02/build/geant4make.sh
    export OFFLINE_MAIN=~/tmp/e1039-core
    export MY_INSTALL=$OFFLINE_MAIN
    export LD_LIBRARY_PATH=$OFFLINE_MAIN/lib:$LD_LIBRARY_PATH
fi
