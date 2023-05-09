# mod_openh264

# FFmpeg cheatsheet

##### Cut using a duration

`ffmpeg -i input.mp4 -ss 10 -t 230 -c:v copy -c:a copy output.mp4`
`ffmpeg -i input.mp4 -ss 00:00:10 -t 00:03:50 -c:v copy -c:a copy output.mp4`

##### Cut using a specific time

`ffmpeg -i input.mp4 -ss 00:00:10 -to 00:04:00 -c:v copy -c:a copy output.mp4`

##### Cut the end of a video

`ffmpeg -sseof -600 -i input.mp4 -c copy output.mp4`
`ffmpeg -sseof -00:10:00 -i input.mp4 -c copy output6.mp4`
