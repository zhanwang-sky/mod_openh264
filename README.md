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

#### Calculate PSNR

$$
MSE=\frac{1}{mn}\sum_{i=0}^{m-1}\sum_{j=0}^{n-1}[I(i,j)-K(i,j)]^2
$$

$$
PSNR=10\cdot\log_{10}{(\frac{MAX_I^2}{MSE})}
$$

`ffmpeg -i distorted.mpg -i reference.mpg -lavfi 'psnr=stats_file=psnr_logfile.txt' -f null -`

`ffmpeg -i distorted-sd.mpg -i reference-hd.mpg -lavfi '[1]scale=-1:720[a];[0][a]psnr=stats_file=psnr_logfile.txt' -f null -`
