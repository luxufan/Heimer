#!/bin/bash

# Builds AppImage in Docker

HEIMER_VERSION=1.3.0

CMD="export LANG=en_US.UTF-8 && \
     export LC_ALL=en_US.UTF-8 && \
     rm -rf /heimer-build-appimage && mkdir -p /heimer/build-appimage && \
     cd /heimer/build-appimage && \
     source /opt/qt*/bin/qt*-env.sh && \
     cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr && \
     make -j$(nproc) && \
     make DESTDIR=appdir -j$(nproc) install && find appdir/ && \
     wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"  && \
     chmod a+x linuxdeployqt-continuous-x86_64.AppImage  && \
     unset QTDIR && unset QT_PLUGIN_PATH && unset LD_LIBRARY_PATH  && \
     export VERSION=${HEIMER_VERSION} && \
     ./linuxdeployqt-continuous-x86_64.AppImage appdir/usr/share/applications/*.desktop -bundle-non-qt-libs && \
     ./linuxdeployqt-continuous-x86_64.AppImage appdir/usr/share/applications/*.desktop -appimage"

if [ -f /.dockerenv ]; then
    echo "Script inside Docker"
    bash -c "${CMD}"
else
    echo "Script outside Docker"
    docker run --privileged -t -v $(pwd):/heimer juzzlin/qt5:14.04 bash -c "${CMD}"
fi

