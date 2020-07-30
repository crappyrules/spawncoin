# daemon runs in the background
# run something like tail /var/log/spawnd/current to see the status
# be sure to run with volumes, ie:
# docker run -v $(pwd)/spawnd:/var/lib/spawnd -v $(pwd)/wallet:/home/spawncoin --rm -ti spawncoin:0.2.2
ARG base_image_version=0.10.0
FROM phusion/baseimage:$base_image_version

ADD https://github.com/just-containers/s6-overlay/releases/download/v1.21.2.2/s6-overlay-amd64.tar.gz /tmp/
RUN tar xzf /tmp/s6-overlay-amd64.tar.gz -C /

ADD https://github.com/just-containers/socklog-overlay/releases/download/v2.1.0-0/socklog-overlay-amd64.tar.gz /tmp/
RUN tar xzf /tmp/socklog-overlay-amd64.tar.gz -C /

ARG SPAWNCOIN_BRANCH=master
ENV SPAWNCOIN_BRANCH=${SPAWNCOIN_BRANCH}

# install build dependencies
# checkout the latest tag
# build and install
RUN apt-get update && \
    apt-get install -y \
      build-essential \
      python-dev \
      gcc-4.9 \
      g++-4.9 \
      git cmake \
      libboost1.58-all-dev && \
    git clone https://github.com/spawncoin/spawncoin.git /src/spawncoin && \
    cd /src/spawncoin && \
    git checkout $SPAWNCOIN_BRANCH && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_CXX_FLAGS="-g0 -Os -fPIC -std=gnu++11" .. && \
    make -j$(nproc) && \
    mkdir -p /usr/local/bin && \
    cp src/TurtleCoind /usr/local/bin/TurtleCoind && \
    cp src/walletd /usr/local/bin/walletd && \
    cp src/zedwallet /usr/local/bin/zedwallet && \
    cp src/miner /usr/local/bin/miner && \
    strip /usr/local/bin/TurtleCoind && \
    strip /usr/local/bin/walletd && \
    strip /usr/local/bin/zedwallet && \
    strip /usr/local/bin/miner && \
    cd / && \
    rm -rf /src/spawncoin && \
    apt-get remove -y build-essential python-dev gcc-4.9 g++-4.9 git cmake libboost1.58-all-dev && \
    apt-get autoremove -y && \
    apt-get install -y  \
      libboost-system1.58.0 \
      libboost-filesystem1.58.0 \
      libboost-thread1.58.0 \
      libboost-date-time1.58.0 \
      libboost-chrono1.58.0 \
      libboost-regex1.58.0 \
      libboost-serialization1.58.0 \
      libboost-program-options1.58.0 \
      libicu55

# setup the spawnd service
RUN useradd -r -s /usr/sbin/nologin -m -d /var/lib/spawnd spawnd && \
    useradd -s /bin/bash -m -d /home/spawncoin spawncoin && \
    mkdir -p /etc/services.d/spawnd/log && \
    mkdir -p /var/log/spawnd && \
    echo "#!/usr/bin/execlineb" > /etc/services.d/spawnd/run && \
    echo "fdmove -c 2 1" >> /etc/services.d/spawnd/run && \
    echo "cd /var/lib/spawnd" >> /etc/services.d/spawnd/run && \
    echo "export HOME /var/lib/spawnd" >> /etc/services.d/spawnd/run && \
    echo "s6-setuidgid spawnd /usr/local/bin/TurtleCoind" >> /etc/services.d/spawnd/run && \
    chmod +x /etc/services.d/spawnd/run && \
    chown nobody:nogroup /var/log/spawnd && \
    echo "#!/usr/bin/execlineb" > /etc/services.d/spawnd/log/run && \
    echo "s6-setuidgid nobody" >> /etc/services.d/spawnd/log/run && \
    echo "s6-log -bp -- n20 s1000000 /var/log/spawnd" >> /etc/services.d/spawnd/log/run && \
    chmod +x /etc/services.d/spawnd/log/run && \
    echo "/var/lib/spawnd true spawnd 0644 0755" > /etc/fix-attrs.d/spawnd-home && \
    echo "/home/spawncoin true spawncoin 0644 0755" > /etc/fix-attrs.d/spawncoin-home && \
    echo "/var/log/spawnd true nobody 0644 0755" > /etc/fix-attrs.d/spawnd-logs

VOLUME ["/var/lib/spawnd", "/home/spawncoin","/var/log/spawnd"]

ENTRYPOINT ["/init"]
CMD ["/usr/bin/execlineb", "-P", "-c", "emptyenv cd /home/spawncoin export HOME /home/spawncoin s6-setuidgid spawncoin /bin/bash"]
