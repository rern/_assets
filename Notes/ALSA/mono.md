```
pcm.!default mono

pcm.mono {
    type route
    slave.pcm "hw:0"
    ttable {
		0.0 0.5 # in ch0, out ch0, vol 50%
		0.1 0.5 # in ch0, out ch1, vol 50%
		1.0 0.5
		1.1 0.5
    }
}
```
