## Mandatory Apple Redistribution 

A for funsies hack that intercepts video rendering calls made by any process, replaces displayed frames with Bad Apple (a meme to get this music video to play on anything, now you can only play it)

## Inspiration
We wanted to hack something pointless but technically cool - like getting doom to run on a calculator. A few popular libs do all the heavy lifting for video but no one messes with them directly. So we thought: what if we could hijack any video and force it to play Bad Apple instead (ie imagine you go to Canvas to watch the latest recorded lecture drop for the class you keep sleeping through, you click on the video and it seems like your professor uploaded Bad Apple?? Then you go to YouTube to watch the latest KRAZAM drop, an ad pops up and it's Bad Apple, you think "what are the chances of that happening" then you click skip ad and it seems like KRAZAM uploaded a music video too????). This is also a less popular meme than the "play doom on X" but if you search youtube you will find hundreds of videos of playing bad apple on X (where X is something like a pregnancy test or an apple), well now you have no choice to play bad apple on everything

## What it does
Hooks into ffmpeg, the video decoding lib that pretty much everything uses - chrome, firefox, vlc, you name it. We intercept the video data right as it's about to be decoded and swap in bad apple frames instead. Works with ffplay, vlc, and nonsandboxed browsers - basically anything using ffmpeg (which is most things) becomes a bad apple player whether it wants to or not

## How we built it
A target application (like ffplay) requests the gpu to render a frame. Our injected shared library intercepts that call before it reaches the gpu. We swap or modify the video data on the fly. The result is displayed as if nothing unusual happened

## Challenges we ran into
Browser security issues, finagling browser configs to choose to use ffmpeg decoding libs, some segfault-ing, finding the common denominator of video rendering calls 

## Accomplishments that we're proud of
Getting bad apple to play over any ffmpeg-based video player without modifying the actual player

## What we learned
Browser security features are annoying

## What's next
Originally we thought this project only had meme value but after working on it we think it would be way cooler to make the system more seamless and configurable and dare I say useful. We plan to replace the current LD_PRELOAD approach with an eBPF-based hook, allowing the interception to work transparently across different applications without manual setup
 
The potential we see is from adding features to any video player regardless of what they support. Stuff like this pops into our minds: 
- dynamic overlays: layering content in real time on top of existing video streams like a subway surfer vid to look at while watching lectures
- real time captioning: intercepting audio, transcribing it using Whisper, and rendering subtitles directly into the video output
- universal compatibility: identifying common gpu/va-api and ffmpeg call paths to support the vast majority of browsers, media players, and streaming apps out-of-the-box
- library mode: turning this into a lightweight and configurable c++ lib for creative or accessibility focused video manipulation that is system wide

==> we just think enabling developers to programmatically remix, annotate, and transform visual streams system wide in flight is a cool idea
