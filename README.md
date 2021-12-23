# mmx-node

## Dependencies

cmake build-essential libsecp256k1-dev libsodium-dev zlib1g-dev ocl-icd-opencl-dev

### OpenCL for Intel iGPUs

Ubuntu 20.04, 21.04
```
sudo apt install intel-opencl-icd
```

Ubuntu ppa for 18.04, 20.04, 21.04
```
sudo add-apt-repository ppa:intel-opencl/intel-opencl
sudo apt update
sudo apt install intel-opencl-icd
```

### OpenCL for AMD iGPUs

Linux:
https://www.amd.com/en/support/kb/release-notes/rn-amdgpu-unified-linux-21-20
https://www.amd.com/en/support/kb/release-notes/rn-amdgpu-unified-linux-21-30

```
./amdgpu-pro-install -y --opencl=pal,legacy
```

Windows: https://google.com/search?q=amd+graphics+driver+download

### OpenCL for Nvidia GPUs

Install CUDA, may the force be with you:
https://www.google.com/search?q=nvidia+cuda+download
