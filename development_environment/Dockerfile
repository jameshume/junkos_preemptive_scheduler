FROM ubuntu:jammy
SHELL ["/bin/bash", "-c"]

RUN --mount=type=tmpfs,target=/build_temp \
    --mount=type=bind,source=.,target=/build \
       apt-get update --fix-missing \
    && apt-get -y upgrade \
    && apt-get install -y --no-install-recommends make \
    && DEBIAN_FRONTEND=noninteractive make -C build -f Dockerfile.Makefile TEMP_DIR=/build_temp all

ENV PATH "$PATH:/opt/gcc-arm-none-eabi/bin"