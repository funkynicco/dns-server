FROM alpine AS build

# CPU cores used to build with (set by build-docker.py based on computers cpu cores * 2)
ARG CPU_CORES=32

# install /bin/bash and nano for debugging
RUN apk add --no-cache bash nano

# install build toolset (gcc, libc-dev, binutils, ... and others)
RUN apk add build-base

#############################################

# build premake5
WORKDIR /setup/premake
ADD setup/premake-5.0.0-beta1-src.tar ./

RUN make -f Bootstrap.mak linux
RUN cp bin/release/premake5 /usr/local/bin

#############################################

WORKDIR /src

COPY premake5.lua .
COPY premake5-system.lua .
COPY libraries ./libraries
COPY projects ./projects

# build nativelib
WORKDIR /src/libraries/nativelib
RUN premake5 gmake
RUN make -j $CPU_CORES config=release_x64
RUN cp build/gmake/bin/nativelib/x64/Release/libnativelib.a build/

# build dns server
WORKDIR /src
RUN premake5 gmake
RUN make -j $CPU_CORES config=release_win64

#############################################
#############################################
#############################################
#############################################

FROM alpine

# install libstdc++ dependency as required by compiled gcc program
RUN apk add --no-cache libstdc++

# install /bin/bash and nano for debugging
RUN apk add --no-cache bash nano

WORKDIR /app
COPY --from=build \
    /src/build/gmake/bin/Server/Win64/Release/clusterdns \
    .

CMD ["./clusterdns", "server"]
