# UDPDK

UDPDK is a minimal UDP/IP stack based on DPDK for fast point-to-point communication between hosts.  
UDPDK API is POSIX to facilitate porting of existing applications.

## Install dependencies

```
git submodule init
git submodule update
```

### DPDK

TODO

### inih

```
cd inih
meson build
cd build
ninja
```

## Install UDPDK

```
cd udpdk
make
sudo make install
```

