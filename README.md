# Example using the Game Boy Four Player Adapter (DMG-07) in C/GBDK-2020.
Den of Snakes is a simple 4 player snake game for the Game Boy.

![Photograph of four Game Boy and clone consoles connected via the Four Player Adapter](/info/game_boy_four_player_consoles.gif)

![Screenshot of the game title screen with the text Den of Snakes, and a drawing of four snakes emerging from the DMG-07 four player adapter ports. Each snake has a different facial expression.](/info/game_boy_4_player_den_of_snakes_title_3x.png)

# Download ROM
Download at: https://bbbbbr.itch.io/den-of-snakes-four-player

# License:
- Graphical assets: CC-BY-NC-ND, meaning no commercial use or derivatives without explicit permission.
- Source code: Public domain

Thinking about making a game that uses the 4 Player adapter? You are welcome (and encouraged!) to use the source code in your project. The main source files of interest are [4_player_adapter.c](src/4_player_adapter.c) and [4_player_adapter.h](src/4_player_adapter.h)


# Game Implementation
In this game the approach to solve synchronization across four consoles uses "deterministic lockstep".
- The game runs in exact lock-step across all the consoles aside from display timing.
- __All__ input is routed through the 4-player adapter and __only__ used after being received from it (by all consoles at the same time).
- The game state only ticks when 4-player packets are received (~about once per frame with the configured timing, so it adds 1 frame of input lag).

As a result of this, consoles broadcast a keep-alive heart beat when not sending button events. Due to the parallel nature, if a player dies on one console then they also die in the exact same manner/location/game timing on all the other consoles.

This approach will not be suitable for all games- however the example hardware interface code is agnostic to how games are implemented.

# System Requirements
- Hardware: In order to play this game on hardware you need at least one Game Boy (preferably more!) and a Nintendo DMG-07 Four Player Adapter.
- Emulators TODO: Double Cherry? GBE+?

# Gameplay
Objective: Be the last snake alive!

Eating hearts grows your snake and deposits skulls. When a snake crashes into skulls or another snake then that snake/player dies and their snake is turned into skull bones. The game ends when only a single player is left alive, that player wins.

When only one player is connected to the DMG-07 then the objective is just to make as long a snake as possible.

Tip: You can enter the Konami code on the title screen of any participating console and the game will run in an alternate Snafu-like gameplay mode.

# Controls
- Move Snake: D-PAD
- Pause: START
- Start Game: START (on any connected console)


# Issues with powered-off GBAs
When GBA-SP (and possibly original GBA) model Game Boys are connected to the DMG-07 Four Player adapter but have their power turned off **and** a cart inserted, they will interfere with the Transmission mode (forcing a return to Ping mode).

So, when those models are powered-off, make sure to disconnect them from the Four Player adapter.

This may apply to other Game Boy models or FPGA clones.


# Bare ISR vector vs GBDK ISR dispatcher
In this example a fixed serial ISR is used (`ISR_VECTOR()`) instead of the default GBDK serial interrupt dispatcher (`add_SIO()`) with installable ISR handlers since it has slightly lower call overhead.

The 4 Player code can be switched to the standard GBDK dispatcher by commenting out the `#define FOUR_PLAYER_BARE_ISR_VECTOR` toggle.


# Source code usage and attribution
The source code is released to the public domain and may be used (please do!) for your own programs. Attribution is welcome but not required.


# Documentation
Additional hardware details found as a result of this example implementation have been submitted to Pandocs page about the 4 Player Adapter.


# Data captures
Saleae logic analyzer [captures of the DMG-07 protocol on the wire](hardware_data_and_notes/logic_analyzer_captures) are included for reference.


# Thanks & Credits
Many thanks to [Shonumi](https://shonumi.github.io/articles/art9.html) for their original research and documentation on this device. It made implementing support tremendously easier.

