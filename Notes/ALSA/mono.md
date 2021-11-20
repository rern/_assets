```
pcm.!default makemono

pcm.makemono {
    type route
    slave.pcm "hw:0"
    ttable {
        0.0 1    # in-channel 0, out-channel 0, 100% volume
        1.0 1    # in-channel 1, out-channel 0, 100% volume
    }
}
```
