// Observations vs Pandocs / DanDocs / etc


 - Leaving powered-off consoles plugged into the adapter but non-powered
   - can cause the 4 Player adapter to detect false positive 0xFF sequence
     - which causes it to quit Transmission mode and restart ping

- Does GB need be connected to Player 1 for DMG-07 to work, since that is where it draws power
  - Or can it draw power from any 3rd party link cable with power wired up? connector?

- The "ping" (ping) diagram for what replies get sent is ambiguous
  - it can be read as ACK1 should be in SB_REG when the Header byte comes in
    - so that it's loaded and ready to be exchanged when the header byte is received
      - but that isn't how it seems to work, instead
        - ACK1 should be loaded into SB_REG *after* receiving Header (0xFE)
          - so that it's received at the same time STATUS_1
            - and so on
  - The **Last** Speed and Size values sent before the 0xAA switch to Transmission mode command
    - determine what the speed and size will be in Transmission mode

  - Missing that all Players will receive 4 x 0xCC signal during Ping/Ping phase
    - when (usually) Player 1 sends 4 x 0xAA to initiate the switch from Ping -> Transmission

  - Any Player can send the 0xAA mode switch request, not just Player 1
    - Not certain, but 0xAA command may need to be aligned
      - such that the first 0xAA is in reply to the Ping header (0xFE)
    - Sending 4 x 0xAA command should be aligned
      - such that it starts being loaded as the next reply immediately after receiving the header packet
        - which means they start being sent one byte after the header for four bytes

  - "It’s possible to restart the ping phase while operating in the transmission phase. To do so, the master Game Boy should send 4 or more bytes (FF FF FF FF, it’s possible fewer $FF bytes need to be sent, but this has not been extensively investigated yet). "
    - It appears sufficient to send as few as 3 x 0xFF to restart Ping mode
    - The 4 x 0xFF command to switch back to Ping mode appears to be more reliable and have less adverse affects
      - when aligned as the first reply bytes to the start of a data packet
        - if this isn't done it can result in a state where the adapter sends garbled data and an altered speed
    - Switching back to Ping mode with 4 x 0xFF seems to always result in a larger gap between bytes (i.e. change in overall transmission rate)
  - Missing that all other Players (usually 2-4) will receive 4 x 0xFF during Ping/Ping phase
    - when (usually) Player 1 sends 4 x 0xFF during Transmission mode to restart Ping mode
      - However Player 1 MUST be connected for any other player to start transmission mode
        - UPDATE!!: NOT 4 x 0xFF, it will be PACKET_BYTES x 0xFF (as in, entire packet is 0xFF)
          - This avoids the false positive situation where a DMG-07 with some consoles unplugged
            - may send periodic 4 x 0xFF even though a mode switch is not being performed
          - Note: This behavior breaks if requesting 5 x 4 = 20 byte packets (the size seems to not be supported)
            - It sends intermittent FF's in groups, but not the full count


  - Power is supplied to the adapter from the Player 1 port
    - Losing power on that port will cause a reset of the device

  - It's possible that any repeating 3(?) byte TX sequence could trigger a reset?
    - seems to be happening when sending tx data byte + 3 x 0xA5 or 3 x 0x01

  - GBs are ALWAYS external clock
    - so when docs say "The protocol is simple: Each Game Boy sends a packet to the DMG-07 simultaneously"
      - They mean "WHEN the GB RXs a packet, it should reply with 


  - The transmission rate is not continuous
    - There seems to be some kind of larger interval between groups of transmitted bytes
      - The size looks variable based on the speed (large gap with faster speed, smaller gap with slower speed)
    - Speed 0x00 -> 13 scanlines between RX bytes -> ((1000 ÷ 59.7) ÷ 254) × 13 = 0.86 msec
      - docs calc: msec per byte: (1000 / ((4194304 ÷ ((6 × 0) + 512)) ÷ 8)) = 0.977 msec
    - Speed 0xFF -> 23 scanlines between RX bytes -> ((1000 ÷ 59.7) ÷ 254) × 23 = 1.57 msec
      - docs calc: msec per byte: (1000 / ((4194304 ÷ ((6 × 255) + 512)) ÷ 8)) = 3.895 msec

  - With F1 race the SPEED setting seems to apply *INSTANTLY* in Ping mode
    - On the first Ping reply it sets SPEED to 0x28
      - Which changes the interval between Ping packets from 12.3ms to 20.2ms

  - Some notes about the the `RATE` byte in the Ping reply, during Ping phase.  (SPEED)
    - The RATE change applies __immediately__ upon next Ping packet
    At least for Ping phase, this formula for RATE at least *does not* apply:
    > `DMG-07 Bits-Per-Second --> 4194304 / ((6 * RATE) + 512)`The lowest setting (RATE = 0) runs the DMG-07 at the normal speed DMGs usually transfer data (1KB/s), while setting it to $FF runs it close to the slowest speed (2042 bits-per-second).
    - Instead RATE controls: **Interval Between Packets** = `(12.2 msec) + ((RATE & 0x0F) * 1 msec)`
    - Yielding a min interval of `12.2 msec` and max of ~`27.21 msec`
    - The timing for a Ping phase packet is broken down like this:
      - **Interval Between Packet Bytes**: __Always__ `1.43 msec` (i.e. between 1->2, 2->3, 3->4)
      - **Clock Period** for packet bytes (while transmitting): __Always__ `15.95 usec` (62.66khz)
      - **Interval Between Packets**: `12.2 msec` - `27.21 msec`
    - I've seen the base value for RATE vary between `12.20 msec` and `12.23 msec`
    - `RATE == 0x00` seems to have behavior where it may not always change the speed, so if one wanted the minimal speed then using a value of `0x10` for example would set that more reliably

  - RATE and Transmission phase timing
    - **Byte Xfer time**:  `~0.128 msec` (Always)
    - **Interval Between Packet Bytes**:  `((RATE >> 4) x .106 msec) + 0.887 msec`
    - **Total Packet Time**... __whichever is larger of the two__:
      - Precalc minimum: `((RATE & 0x0F) x 1 msec) + 17 msec`
      - Size Based Transfer time `(Byte Xfer + Interval Between Packet Bytes) x Byte Count) + (a value that fluctuates between .36 to 2.15 msec)`
        - Not sure how that trailing add-on amount gets calculated
        - This scenario where Size Based Transfer time exceeds the precalc minimum mostly shows up in 16 byte packet settings, or smaller packet sizes when the *Interval Between Packet Bytes* is at the largest setting.


