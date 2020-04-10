# UpNext: A glanceable eInk calendar for your desk

UpNext sits on your desk as an unobstusive way to make sure you always make your next meeting.
It is based on a [Rasberry Pi](https://www.raspberrypi.org/) connected with a [Waveshare eInk display](https://www.amazon.com/4-2inch-Resolution-Two-Color-Interface-Raspberry/dp/B07PKSLR3W/ref=sr_1_2?dchild=1&keywords=waveshare+4.2inch+epaper&qid=1584221897&s=electronics&sr=1-2), and pulls events from Google Calendar.

The code is written (poorly) in C++, because writing to the eInk display is timing-sensitive, there were good drivers for C++, and it was a good excuse to dust off my "lower level" programming skills after a decade of primarily programming in python and javascript.

Full writeup at http://brettcvz.com/projects/6-upnext

## Building - Abbreviated version
Requires the following libraries to build:
- [Pango](https://pango.gnome.org/) + [Cairo](https://www.cairographics.org/)
- [restclient-cpp](https://github.com/mrtazz/restclient-cpp#installation)
- [bcm2835](https://www.airspayce.com/mikem/bcm2835/)

Be sure to also:
- Rename `secrets.h.example` to `secrets.h` and add in your google calendar api keys
- Create a `bin`, `bld`, and `logs` directory
- Enable the SPI interface on the raspberry pi
- [Install](https://wiki.debian.org/Fonts#Adding_fonts) the [Proxima Nova](https://fonts.adobe.com/fonts/proxima-nova) font, or [substitute your favorite](https://wiki.archlinux.org/index.php/fonts#List_all_installed_fonts). Note that figuring out which fonts look good on the screen requires some testing.

## Building - detailed, from fresh Rasberry Pi version
Starting from a fresh SD card with [Rasbian Lite](https://www.raspberrypi.org/downloads/raspbian/):

### Basic Rasberry Pi Config
`raspi-config`:
 - change password and hostname
 - expand filesystem under advanced
 - setup locales
 - set timezone
 - enable ssh and SPI under interfacing options
 - setup WIFI

Reboot and confirm wifi works (`ping www.google.com`)

Setup ssh:
 - From "host" computer: `ssh pi@<hostname>.local`, log in with password
 - Copy `id_dsa.pub` from host computer into `.ssh/authorized_keys` in pi
 - Exit ssh, ssh back in again, ensure you don't get prompted for a password
 - edit `/etc/ssh/sshd_config` to disable password login
 - `sudo systemctl restart ssh`
 - Test everything is configured by trying `ssh test@<hostname>.local` - should be blocked, no password prompt

```
sudo timedatectl set-ntp True # to fix clock skew
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install vim git screen
```

### Installing dependencies
#### Pangocairo
```
sudo apt-get install libcairo2-dev libpango1.0-dev libpangocairo-1.0-0
```

#### Restclient-cpp
```
git clone https://github.com/mrtazz/restclient-cpp
cd restclient-cpp
sudo apt-get install automake libtool libcurl4-openssl-dev
./autogen.sh
./configure
sudo make install
```

#### BCM2835
```
wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.63.tar.gz
tar -xzvf bcm2835-1.63.tar.gz
cd bcm2835-1.63/
./configure
make
sudo make check
sudo make install
```

#### Fonts
[Install](https://wiki.debian.org/Fonts#Adding_fonts) the [Proxima Nova](https://fonts.adobe.com/fonts/proxima-nova) font, or [substitute your favorite](https://wiki.archlinux.org/index.php/fonts#List_all_installed_fonts). Note that figuring out which fonts look good on the screen requires some testing.

Put the fonts in `/usr/local/share/fonts`, then:
```
fc-cache -fv
```

### Building upNext
```
git clone https://github.com/brettcvz/upnext.git upNext
cd upNext
mv code/secrets.h.example code/secrets.h
vi code/secrets.h
sudo ldconfig
make build
mkdir logs
```

### Installing init.d script to launch on boot
```
sudo cp init-d-boot-script.sh /etc/init.d/upNext
sudo systemctl enable upNext
```

Done! Restart and you should be good to go. If one of these steps didn't work for you, please create an issue describing what went wrong and I'll see if I can help.

## References
Much of the code for interfacing with the e-Paper module is based on the manufacturer's [sample code](https://github.com/waveshare/e-Paper) and [documentation](https://www.waveshare.com/wiki/4.2inch_e-Paper_Module_(B))

I borrowed a lot from the LUT tables from [ZinggJM](https://github.com/ZinggJM/GxEPD), in particular [this one](https://github.com/ZinggJM/GxEPD/blob/master/src/GxGDEW042T2/GxGDEW042T2.cpp) to get partial refreshes working well. In order to build a deeper understanding of e-Ink LUT tables and refresh rate, I leaned a lot on [this video](https://www.youtube.com/watch?v=MsbiO8EAsGw&feature=youtu.be)
