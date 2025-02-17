# USBCamera

## Install lib

```
sudo apt update
sudo apt install libopencv-dev
sudo apt install gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav
```

## Build and run

```
  make run
```

## Build

```
  make
  export LD_LIBRARY_PATH=linux:$$LD_LIBRARY_PATH
  ./main
```

