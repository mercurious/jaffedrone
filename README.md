# Jaffedrone

A five-string drone synthesizer for the Miyoo Mini Plus running OnionOS.

Each string is a Karplus-Strong loop with the Jaffe-Smith extensions: an
allpass for fine tuning, a damping filter for brightness, and a loop gain for
decay. A noise "bow" keeps the strings ringing, held in check by each loop's
own level, and strings can re-pluck on a tempo grid. The five go through a
master lowpass and a reverb.

## Controls

STRINGS page: left/right picks a string, up/down a parameter, Y/X adjusts,
A toggles the string, B plucks it. DRONE page: up/down and Y/X edit the
global parameters, B plucks everything. SELECT deals a new chord, START
starts or stops the drone, MENU exits.

## Build

`make` cross-compiles `dist/Jaffedrone/jaffedrone` with zig against Onion's
own SDL 1.2. `make test` renders a WAV and screenshots into `build/test` on
the host. `make install SD=/path/to/card` copies the app into `App/`.
