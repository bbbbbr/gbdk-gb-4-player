# Example Game Boy Four Player Adapter (DMG-07) support in C/GBDK-2020.

Work in Progress

Status:
- 4-player adapter: Protocol implemented and entirely working. Code in src: [4_player_adapter.c](src/4_player_adapter.c) and [4_player_adapter.h](src/4_player_adapter.h)
- Gameplay: Basic gameplay only at present. Player input wired up and working, snakes grow, snakes collide, game can end.

![Photograph of four Game Boy and clone consoles connected via the Four Player Adapter](/info/game_boy_4_player_consoles.gif)

# Game Implementation

In this game the approach to solve potential synchronization issues across four consoles is as follows.
- The game runs in exact lock-step across all the consoles aside from display timing.
- __All__ input is routed through the 4-player adapter and __only__ used after being received from it.
- The game state only ticks when 4-player packets are received (~about once per frame with the configured timing, so it adds 1 frame of input lag).

As a result of this, for the most part, consoles do not broadcast events to each other aside from button inputs. Instead if a player dies on one console, it is essentially guaranteed (as long as there is no packet loss) that the player dies in the same exact manner/location/game timing on all the other consoles.

This approach will not be suitable for all games- however the example hardware interface code is agnostic to how games are implemented.


In this example a fixed serial ISR is used (`ISR_VECTOR()`) instead of the default GBDK serial interrupt dispatcher (`add_SIO()`), however that could easily be changed. The fixed approach is used since it has slightly lower overhead.


The source code is released to the public domain and may be used (please do!) for your own programs.

# Documentation

Additional hardware details found as a result of this example implementation have been submitted to Pandocs and may become available there.


# Data captures

Saleae logic analyzer [captures of the DMG-07 protocol on the wire](hardware_data_and_notes/logic_analyzer_captures) are included for reference.

# Thanks & Credits

Many thanks to [Shonumi](https://shonumi.github.io/articles/art9.html) for their original research and documentation on this device. It made implementing support tremendously easier.

