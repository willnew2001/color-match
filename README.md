# color-match

Video example: https://www.youtube.com/watch?v=h-BYgH5R9Lc

The goal of the game is to match the color of the right rectangle with the 
color of the left rectangle. The left rectangle is a random color during every 
round, and you control the right rectangle with the orientation of the Nunchuck 
controller (using data from the accelerometer). If you hold the orientation of the 
controller on the correct color for a certain amount of time, you've gotten a match, 
and the game moves on to the next round, generating a new random color for you to 
match. There are 3 rounds in this example. At the end, you get to see how long 
it took you. There's a certain threshold for the correct color since it's hard to
match the exact RGB for an extended period of time.

This program was written on a Raspberry Pi 4b running an installation of Plan9 for
a university class final project. Due to the OS being Plan9, the version of C written
is slightly different than traditional. You might notice 'print' instead of 'printf',
for example. You will not be able to compile this code using gcc.

Here's the peripherals I used for this project:

Wii Nunchuck I2C adapter: https://www.adafruit.com/product/4836
SPI LCD screen: https://www.adafruit.com/product/358
