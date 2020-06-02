FROM tee-sdk:latest

RUN apt-get update \
    && apt-get install openjdk-8-jdk unzip zip libx11-dev libxext-dev libxrender-dev libxtst-dev libxt-dev libcups2-dev libfreetype6-dev libasound2-dev libfontconfig1-dev -y \
    && apt-get clean cache

COPY ./ /uranus

RUN ln -s /tee-sdk/build/linux /uranus/hotspot/lib

RUN mkdir -p /uranus/hotspot/build/

WORKDIR /uranus/hotspot/build/

RUN cmake ../ && make -j && make install && make install && cp libenclave.so /usr/lib/

WORKDIR /uranus

RUN ./configure

RUN make images
