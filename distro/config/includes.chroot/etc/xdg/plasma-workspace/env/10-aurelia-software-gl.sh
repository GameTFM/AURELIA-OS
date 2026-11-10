#!/bin/sh
# AURELIA: virtual machines and systems whose GPU exposes no DRM render node
# cannot create an EGL context. Qt then reports "libEGL warning: egl: failed to
# create dri2 screen" and KWin aborts the session with
#   "The Wayland connection broke. Did the Wayland compositor die?"
# which leaves the desktop frozen. Fall back to Mesa's software rasteriser and
# the Qt Quick software backend in that case only, so machines with a working
# GPU keep their hardware acceleration.

if [ ! -e /dev/dri/renderD128 ]; then
    export LIBGL_ALWAYS_SOFTWARE=1
    export QT_QUICK_BACKEND=software
    export KWIN_COMPOSE=Q
    export QTWEBENGINE_CHROMIUM_FLAGS="${QTWEBENGINE_CHROMIUM_FLAGS:-} --disable-gpu"
fi
