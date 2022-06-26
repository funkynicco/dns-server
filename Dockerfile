FROM alpine AS build

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

RUN premake5 gmake
RUN make config=release_win64

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
    /src/build/gmake/bin/Server/Win64/Release/ClusteredDnsServer \
    .

CMD ["./ClusteredDnsServer"]