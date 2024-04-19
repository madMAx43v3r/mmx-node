FROM ubuntu:22.04 AS builder
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade \
		&& apt-get install -y \
			apt-utils \
			git \
			cmake \
			automake \
			libtool \
			build-essential \
			libminiupnpc-dev \
			libjemalloc-dev \
			zlib1g-dev \
			ocl-icd-opencl-dev \
			ccache \
			&& rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN git submodule update --init --recursive
RUN --mount=type=cache,target=/root/.cache/ccache sh make_release.sh "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"

FROM ubuntu:22.04 AS base
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade \
		&& apt-get install -y \
			apt-utils \
			libminiupnpc17 \
			libjemalloc2 \
			zlib1g \
			libgomp1 \
			ocl-icd-libopencl1 \
			curl \
			clinfo \
			&& rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /app/build/dist ./
COPY ["docker-entrypoint.sh", "./"]

ENV MMX_HOME="/data/"
VOLUME /data

# node p2p port
EXPOSE 12341/tcp
# http api port
EXPOSE 11380/tcp

ENTRYPOINT ["./docker-entrypoint.sh"]
CMD ["./run_node.sh"]

FROM base AS amd
ARG AMD_DRIVER=amdgpu-install_5.4.50400-1_all.deb
ARG AMD_DRIVER_URL=https://repo.radeon.com/amdgpu-install/5.4/ubuntu/jammy
RUN mkdir -p /tmp/opencl-driver-amd \
    && cd /tmp/opencl-driver-amd \
    && curl --referer $AMD_DRIVER_URL -O $AMD_DRIVER_URL/$AMD_DRIVER \
	&& dpkg -i $AMD_DRIVER \
	&& rm -rf /tmp/opencl-driver-amd
RUN apt-get update && apt-get -y upgrade \
		&& apt-get install -y \
			rocm-opencl \
			&& rm -rf /var/lib/apt/lists/*

FROM base AS nvidia
RUN mkdir -p /etc/OpenCL/vendors \
    && echo "libnvidia-opencl.so.1" > /etc/OpenCL/vendors/nvidia.icd
ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility
