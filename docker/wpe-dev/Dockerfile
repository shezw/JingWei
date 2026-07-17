# JingWei
# docker wpe-dev Dockerfile    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

FROM ubuntu:24.04@sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90 AS jingwei-dev

ARG DEBIAN_FRONTEND=noninteractive

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        ccache \
        clang \
        cmake \
        curl \
        g++ \
        gcc \
        git \
        libdrm-dev \
        libegl1-mesa-dev \
        libgbm-dev \
        libgles2-mesa-dev \
        libinput-dev \
        libsdl2-dev \
        libvncserver-dev \
        libxkbcommon-dev \
        lld \
        mesa-common-dev \
        mesa-utils \
        ninja-build \
        pkg-config \
        python3 \
    && rm -rf /var/lib/apt/lists/*

FROM jingwei-dev AS wpe-dev

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        autopoint \
        autotools-dev \
        bison \
        bubblewrap \
        dbus \
        flex \
        gawk \
        gettext \
        gperf \
        intltool \
        itstool \
        libavif-dev \
        libbz2-dev \
        libcairo2-dev \
        libenchant-2-dev \
        libepoxy-dev \
        libevent-dev \
        libfontconfig1-dev \
        libfreetype-dev \
        libgcrypt20-dev \
        libglib2.0-dev \
        libgnutls28-dev \
        libharfbuzz-dev \
        libhyphen-dev \
        libicu-dev \
        libjpeg-dev \
        libjxl-dev \
        liblcms2-dev \
        libnghttp2-dev \
        libopenjp2-7-dev \
        libpng-dev \
        libproxy-dev \
        libpsl-dev \
        libseccomp-dev \
        libsecret-1-dev \
        libsoup-3.0-dev \
        libsqlite3-dev \
        libsysprof-capture-4-dev \
        libsystemd-dev \
        libtasn1-6-dev \
        libtool \
        libtool-bin \
        libunwind-dev \
        libwebp-dev \
        libwoff-dev \
        libxml2-dev \
        libxml2-utils \
        libxslt1-dev \
        meson \
        nasm \
        patch \
        patchelf \
        perl \
        python3-psutil \
        python3-setuptools \
        ruby \
        unifdef \
        uuid-dev \
        xz-utils \
        yasm \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends \
        xdg-dbus-proxy \
    && rm -rf /var/lib/apt/lists/*

ARG JW_UID=10001
ARG JW_GID=10001

RUN groupadd --gid "${JW_GID}" jw \
    && useradd --create-home --uid "${JW_UID}" --gid "${JW_GID}" --shell /bin/bash jw \
    && mkdir -p \
        /workspace/JingWei \
        /workspace/jingwei-build \
        /workspace/wpe/build \
        /workspace/wpe/downloads \
        /workspace/wpe/prefix \
        /workspace/wpe/src \
        /workspace/cache/ccache \
    && chown -R jw:jw /workspace

COPY --chown=jw:jw . /workspace/JingWei

ENV CC=clang \
    CXX=clang++ \
    CCACHE_DIR=/workspace/cache/ccache \
    CCACHE_MAXSIZE=20G \
    CMAKE_BUILD_PARALLEL_LEVEL=3 \
    WPE_BUILD_JOBS=3 \
    WPE_BUILD_TYPE=Release \
    WPE_PROFILE=container \
    WPE_VERSION=2.52.5

USER jw
WORKDIR /workspace/JingWei

CMD ["/bin/bash"]

FROM wpe-dev AS wpe-full-dev

USER root

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends \
        flite1-dev \
        gobject-introspection \
        gsettings-desktop-schemas-dev \
        libasound2-dev \
        libatk-bridge2.0-dev \
        libatk1.0-dev \
        libgstreamer-plugins-bad1.0-dev \
        libgstreamer-plugins-base1.0-dev \
        libgstreamer1.0-dev \
        libmanette-0.2-dev \
        python3-gi \
    && rm -rf /var/lib/apt/lists/*

USER jw
