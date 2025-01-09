FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

ENV TZ=Europe/Berlin
RUN apt-get update

# bluerange mesh dependencies
RUN apt-get install -qqy git python3 python3-pip cmake gcc g++ libncurses5-dev build-essential

# emd sdk dependencies 
RUN apt-get install -qqy clang zlib1g zlib1g-dev llvm python
RUN pip3 install requests

WORKDIR /opt
RUN git clone https://github.com/emscripten-core/emsdk.git
WORKDIR /opt/emsdk

# 3.1.74 results in build errors
RUN git checkout 2.0.33

RUN ./emsdk install latest
RUN ./emsdk activate latest

RUN echo "source /opt/emsdk/emsdk_env.sh" >> ~/.bashrc

# necessary for 'source' to work
SHELL ["/bin/bash", "-c"]

COPY . /opt/bluerange-mesh

RUN mkdir -p /opt/bluerange-mesh/build/linux
WORKDIR /opt/bluerange-mesh/build

RUN source /opt/emsdk/emsdk_env.sh && emcmake cmake ../ -DBUILD_TYPE=SIMULATOR -DFM_NATIVE_RENDERER_ENABLED=ON
RUN source /opt/emsdk/emsdk_env.sh && cmake --build . --target cherrySim_runner
RUN source /opt/emsdk/emsdk_env.sh && make -j 8

ENTRYPOINT ["bash", "-c", "python3 /opt/bluerange-mesh/build/Server.py"]

