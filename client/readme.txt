
This is the playstation controller code for the remote RUDEBOT.

Requirements:

libusb for osx - download based on your osx version from here: http://www.ellert.se/twain-sane/
  * "port" version of libusb does *not* work for me with the sixpair tool, needed this one!
  * Needed for sixpair tool
sixpair cli tool - download here: http://www.dancingpixelstudios.com/sixaxiscontroller/sixpair
  * This lets you manually pair a controller to your mac

pygame - can install in osx via "sudo port install Pygame", assuming you have macports installed


Usage:

Should be able to tweak the settings at the top of game.py, and then just run it: python ./game.py

