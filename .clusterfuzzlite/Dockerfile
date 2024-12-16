FROM gcr.io/oss-fuzz-base/base-builder:v1
RUN apt-get update && apt-get install -y make autoconf automake libtool
COPY . $SRC/zforth
WORKDIR $SRC/zforth
COPY .clusterfuzzlite/build.sh $SRC/
