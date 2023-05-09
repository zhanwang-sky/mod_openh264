# mod_openh264

# FFmpeg cheatsheet

#### Cut/trim video

https://shotstack.io/learn/use-ffmpeg-to-trim-video/

##### Cut using a duration

`ffmpeg -i input.mp4 -ss 10 -t 230 -c:v copy -c:a copy output.mp4`

`ffmpeg -i input.mp4 -ss 00:00:10 -t 00:03:50 -c:v copy -c:a copy output.mp4`

##### Cut using a specific time

`ffmpeg -i input.mp4 -ss 00:00:10 -to 00:04:00 -c:v copy -c:a copy output.mp4`

##### Cut the end of a video

`ffmpeg -sseof -600 -i input.mp4 -c copy output.mp4`

`ffmpeg -sseof -00:10:00 -i input.mp4 -c copy output.mp4`

#### Extract h264 raw stream

`ffmpeg -i input.mp4 -vcodec copy -bsf h264_mp4toannexb -an -f {rawvideo|h264|whatever} output.h264`

#### Convert YUV444p to YUV420p

`ffmpeg -i input.mp4 -c:v libx264 -pix_fmt yuv420p output.mp4`
