# DynamicGSTreamerRecord

This is a self contained project. It has a timer which will call the callback every 100 ms.
The purpose of this project is to practice manipulating a gstreamer pipeline.
1. adding and removing elements for recording from a running gstreamer pipeline.
    On the 25th timeout, it will add a recording pipeline to the running pipeline and record the input video to file TIMESTAMP.mp4
    On the 80th timeout, it will add a recording pipeline to the running pipeline and record the input video to file TIMESTAMP.mp4
2. Check qos (quality of service) of a gstreamer pipeline 


compile using
gcc stream_record.c -o record \`pkg-config --cflags --libs gstreamer-1.0\`
