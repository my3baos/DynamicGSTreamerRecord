# DynamicGSTreamerRecord
A Dynamic Gstreamer pipeline example that starts and stops recording on a timer

This is a self contained project. It has a timer which will call the callback every 100 ms.

On the 25th timeout, it will add a recording pipeline to the running pipeline and record the input video to file TIMESTAMP.mp4


On the 80th timeout, it will add a recording pipeline to the running pipeline and record the input video to file TIMESTAMP.mp4



compile using
gcc stream_record.c -o record \`pkg-config --cflags --libs gstreamer-1.0\`
