FROM centos:6

# Default software in cent6 has low version
# Remove all slient mode because network maybe bad

WORKDIR /root

# Lack of (maybe used by tests)
# 1. python2.7 (now is 2.6.6)
# 2. findbugs & ant
# 3. shellcheck
# 4. svn (removed by official)
# 5. nodejs & npm (required by YANR-UI?)

# 1.安装依赖的软件包, 先添加新源, 否则默认缺失库
RUN set -x \
    && yum -q update -y \
    && yum install -y centos-release-scl scl-utils epel-release \
    && yum install -y \
    devtoolset-7-gcc \
    devtoolset-7-gcc-c++ \
    python27 \
    python-pip \
    git \
    doxygen \
    rsync \
    bats \
    ncurses-devel \
    nasm \
    m4 \
    sudo \
    java-1.8.0-openjdk-devel \
    bzip2-devel \
    fuse-devel \
    openssl-devel \
    snappy-devel \
    zlib-devel \
    libzstd-devel \
    && yum clean all  \
    && rm -rf /var/cache/yum
    
ENV JAVA_HOME "/usr/lib/jvm/java-1.8.0-openjdk.x86_64"
ENV PATH "${PATH}:/opt/rh/devtoolset-7/root/usr/bin"

# 2. 开启gcc & g++ V7, 手动安装cmake 3.1.0 (25M)
# 替换源加速下载, 必要需考虑修改hosts文件或再换源
RUN echo "source /opt/rh/devtoolset-7/enable" >> /etc/bashrc && \
    source /etc/bashrc && \
    mkdir -p /opt/cmake && \
    curl -SL https://github.com/Kitware/CMake/releases/download/v3.10.0/cmake-3.10.0-Linux-x86_64.tar.gz \
         -o /opt/cmake.tar.gz && \
    tar xzf /opt/cmake.tar.gz --strip-components 1 -C /opt/cmake && \
    rm -f /opt/cmake.tar.gz
ENV CMAKE_HOME /opt/cmake
ENV PATH "${PATH}:/opt/cmake/bin"

# 3. 手动安装autoconf2.69
RUN mkdir -p /opt/autoconf && \
    curl -SL http://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz \
         -o /opt/autoconf.tar.gz && \
    tar xzf /opt/autoconf.tar.gz --strip-components 1 -C /opt/autoconf && \
    cd /opt/autoconf && \
    ./configure && \
    make && make install && \
    cp ./bin/autoconf /usr/bin && \
    rm -f /opt/autoconf.tar.gz


# 4. 手动安装Protobuf 2.5.0
RUN mkdir -p /opt/protobuf-src && \
    curl -SL https://github.com/google/protobuf/releases/download/v2.5.0/protobuf-2.5.0.tar.gz \
         -o /opt/protobuf.tar.gz && \
    tar xzf /opt/protobuf.tar.gz --strip-components 1 -C /opt/protobuf-src && \
    cd /opt/protobuf-src && ./configure --prefix=/opt/protobuf && \
    make install && rm -f /opt/protobuf.tar.gz
ENV PROTOBUF_HOME /opt/protobuf
ENV PATH "${PATH}:/opt/protobuf/bin"

# 5. 手动安装Maven 3.3.9
RUN mkdir -p /opt/maven && \
    curl -SL https://mirrors.tuna.tsinghua.edu.cn/apache/maven/maven-3/3.3.9/binaries/apache-maven-3.3.9-bin.tar.gz \
         -o /opt/maven.tar.gz && \
    tar xzf /opt/maven.tar.gz --strip-components 1 -C /opt/maven && \
    rm -f /opt/maven.tar.gz
ENV MAVEN_HOME /opt/maven
ENV PATH "${PATH}:/opt/maven/bin"

# 6.安装低版本py2支持的pylint
RUN pip2 install pylint==1.9.2

# 7. 安装dateutil.parser
RUN pip2 install python-dateutil

# 8.安装isal(EC依赖) (RPM require GLIB_2.14)
# https://www.nasm.us/pub/nasm/releasebuilds/2.14.02/linux/nasm-2.14.02-0.fc27.x86_64.rpm
RUN mkdir -p /opt/nasm /opt/isal \
    && curl -SL https://www.nasm.us/pub/nasm/releasebuilds/2.14.02/nasm-2.14.02.tar.gz -o /opt/nasm.tar.gz \
    && tar xzf /opt/nasm.tar.gz --strip-components 1 -C /opt/nasm \
    && cd /opt/nasm && ./autogen.sh && ./configure && make && make install \
    && cp ./nasm /usr/bin \
    && rm -f /opt/nasm.tar.gz

RUN curl -SL https://github.com/intel/isa-l/archive/v2.28.0.tar.gz -o /opt/isal.tar.gz \
    && tar xzf /opt/isal.tar.gz --strip-components 1 -C /opt/isal \
    && cd /opt/isal && make -f Makefile.unx \
    && mv bin/*isa* /usr/lib64/ \
    && rm -rf /opt/*tar.gz 

# 9.优化安装node.js Yarn-UI可能需要, HDFS不需要 (且需改成Cent版)
# RUN apt-get install -y --no-install-recommends nodejs npm \
#     && ln -s /usr/bin/nodejs /usr/bin/node \
#     && npm install npm@latest -g \
#     && npm install -g jshint

# 避免编译时OOM, 设置jvm内存限制
ENV MAVEN_OPTS -Xms256m -Xmx1536m

# 9.运行环境检查脚本, 显示欢迎界面 (结束)
ADD hadoop_env_checks.sh /root/hadoop_env_checks.sh
RUN chmod 755 /root/hadoop_env_checks.sh \
    && echo '~/hadoop_env_checks.sh' >> /root/.bashrc