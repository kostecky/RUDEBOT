RUDEBOT - Rolling Ubiquitous Display Engine for Binary-Organic Transliteration
==============================================================================

This is the arduino code base along with the client for our wireless telepresence engine.

##Instructions

1. Set the appropriate IP information in the RUDEBOT.ino sketch and upload to the arduino
2. Using socat in raw mode, initiate a connection to port 8888
```
    socat -,raw,echo=0,escape=0x03 TCP:XXX.XXX.XXX.XXX:8888,keepalive,nodelay
```
3. Alternatively, you can use the client library with a PS3 gamepad
