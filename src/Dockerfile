FROM gcc:9

ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    make \
    libboost-all-dev \
    libpthread-stubs0-dev \
    libtbb-dev

WORKDIR /app

COPY . /app

RUN make clean && make

ENTRYPOINT ["./app"]