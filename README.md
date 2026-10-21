# Jaffedrone

A five-string drone synthesizer for the Miyoo Mini Plus running OnionOS.

Each string is a Karplus-Strong loop with the Jaffe-Smith extensions: an
allpass for fine tuning, a damping filter for brightness, and a loop gain for
decay. A noise "bow" keeps the strings ringing, held in check by each loop's
own level, and strings can re-pluck on a tempo grid. The five go through a
master lowpass and a reverb.

## Install

Turn on SSH on the Miyoo (Apps > Tweaks > Network > SSH). Then run this on a computer on the
same network:

```sh
curl -fsSL https://raw.githubusercontent.com/mercurious/jaffedrone/main/install.sh | bash
```

The script finds the Miyoo by scanning the local /24 for Onion's SSH server. It checks the
installed version against the latest
[release](https://github.com/mercurious/jaffedrone/releases), and if the release is newer it
copies the update to `SD:/App/Jaffedrone`. Your `jaffedrone.sav` is kept. Run it again any
time to update. Log in with Onion's default `onion` / `onion`. The address that worked is
saved in `~/.config/jaffedrone/host`. To skip the scan, set `JD_HOST`:

```sh
curl -fsSL https://raw.githubusercontent.com/mercurious/jaffedrone/main/install.sh | JD_HOST=192.168.1.28 bash
```

`JD_USER` changes the SSH user. `JD_FORCE=1` reinstalls even when the version matches.
Quit Jaffedrone on the Miyoo before updating. Without Wi-Fi, copy the `Jaffedrone` folder
from the release's `Jaffedrone.zip` to `SD:/App/` on the card.

## Controls

STRINGS page: left/right picks a string, up/down a parameter, Y/X adjusts,
A toggles the string, B plucks it. DRONE page: up/down and Y/X edit the
global parameters, B plucks everything. SELECT deals a new chord without
plucking: the strings keep ringing and slide to the new notes over the
GLIDE time (a tempo division, or OFF for a jump). START starts or stops the
drone, MENU exits.

## Build

`make` cross-compiles `dist/Jaffedrone/jaffedrone` with zig against Onion's
own SDL 1.2. `make test` renders a WAV and screenshots into `build/test` on
the host. `make install SD=/path/to/card` copies the app into `App/`.
`make release` publishes `build/Jaffedrone.zip` as the GitHub release for the
current `JD_VERSION`; tag and push `v<version>` first.

## License

MIT, copyright mercurious and sydilaxe. See [LICENSE](LICENSE).
