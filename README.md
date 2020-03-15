# UpNext: A glanceable eInk calendar for your desk

UpNext sits on your desk as an unobstusive way to make sure you always make your next meeting.
It is based on a [Rasberry Pi](https://www.raspberrypi.org/) connected with a [Waveshare eInk display](https://www.amazon.com/4-2inch-Resolution-Two-Color-Interface-Raspberry/dp/B07PKSLR3W/ref=sr_1_2?dchild=1&keywords=waveshare+4.2inch+epaper&qid=1584221897&s=electronics&sr=1-2), and pulls events from Google Calendar.

The code is written (poorly) in C++, because writing to the eInk display is timing-sensitive, there were good drivers for C++, and it was a good excuse to dust off my "lower level" programming skills after a decade of primarily programming in python and javascript.

## Building
Requires the following libraries to build:
- [Pango](https://pango.gnome.org/) + [Cairo](https://www.cairographics.org/)
- [restclient-cpp](https://github.com/mrtazz/restclient-cpp#installation)
- [bcm2835](https://www.airspayce.com/mikem/bcm2835/)

Be sure to also:
- Rename `secrets.h.example` to `secrets.h` and add in your google calendar api keys
- Create a `bin`, `bld`, and `logs` directory
- Enable the SPI interface on the raspberry pi
- [Install](https://wiki.debian.org/Fonts#Adding_fonts) the [Proxima Nova](https://fonts.adobe.com/fonts/proxima-nova) font, or [substitute your favorite](https://wiki.archlinux.org/index.php/fonts#List_all_installed_fonts). Note that figuring out which fonts look good on the screen requires some testing.

## References
Much of the code for interfacing with the e-Paper module is based on the manufacturer's [sample code](https://github.com/waveshare/e-Paper) and [documentation](https://www.waveshare.com/wiki/4.2inch_e-Paper_Module_(B))

I borrowed a lot from the LUT tables from [ZinggJM](https://github.com/ZinggJM/GxEPD), in particular [this one](https://github.com/ZinggJM/GxEPD/blob/master/src/GxGDEW042T2/GxGDEW042T2.cpp) to get partial refreshes working well. In order to build a deeper understanding of e-Ink LUT tables and refresh rate, I leaned a lot on [this video](https://www.youtube.com/watch?v=MsbiO8EAsGw&feature=youtu.be)
