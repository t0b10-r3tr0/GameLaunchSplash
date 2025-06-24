# GameLaunchSplash
A simple game launch splash screen display for Batocera-like distributions.

## Written in C++ using SDL 2 and OpenGL ES 2
* lightweight
* portable
* performant

## How to Build
```shell
# build regular version
g++ GameLaunchSplash.cpp -o game_launch_splash -lSDL2 -lGLESv2

# build simple version
g++ GameLaunchSplashNoFadeOut.cpp -o game_launch_splash_simple -lSDL2 -lGLESv2
```

## How to Run
```./game_launch_splash <image.png> <initial_scale> <final_scale> <fade_time> <show_time> [background-image.png]```

### Example
```shell
## with background image
./game_launch_splash roms/snes/images/rom_name-marquee.png 0 1 0.666 0 roms/snes/images/rom_name-fanart.jpg

## no background imager
./game_launch_splash roms/snes/images/rom_name-marquee.png 0 1 0.666 0
```

## Thanks
Thanks to the community for being inclusive, welcoming, and kind.

Special shout out go to these amazing folks who have been more than helpful along the way:
* Christian Haitian
* Cebion
* Kloptops
* Bamboozler
* Benaimino
* Schmurtz
* Acme Plus
* Chrizzo
* RetroGamingMonkey