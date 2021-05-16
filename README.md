# DynamicGSTreamerRecord
A Dynamic Gstreamer pipeline example that starts and stops recording on a timer

This is a self contained project. It has a timer which will call the callback every 100 ms. \n

On the 25th timeout, it will add a recording pipeline to the running pipeline and record the input video to file aaa.mp4
On the 75th timeout, it will unlink the recording pipeline.

On the 80th timeout, it will add a recording pipeline to the running pipeline and record the input video to file bbb.mp4
On the 120th timeout, it will unlink the recording pipeline
