# (28-02-2026) First try at building a loop antenna

Built a rectangle, 18.5mm by 14mm, and calculated using https://miguelvaca.github.io the rough capacitor (560pF), was able to observer very slight reduction S11 near the expected resonant frequency (around 10MHz, measuring touching the two ends of the capacitor), but only of around -1.1dB, so pretty much nothing.

I used 3mm copper tape, which was probably too lossy (multimeter measured 0.1ohm but that can't really be trusted at RF), and then Q factor was destroyed, the secondary picked up absolutely nothing.

Tried again with a 10pF cap, which would theoretically give 30MHz resonance, but still nothing!

Next steps: Make a Mathematica calculator to find the minimum conductor area, and buy some copper refrigeration pipe.

# (Don't remember date, mid-March) Second try at building a loop antenna

Bought copper pipe from Leroy Merlin, filled it with salt and bent it into a rough 50cm x 50cm square. Did a lot of calculations in Mathematica to estimate Q, and for my design it's satisfactory.

Found some nails that after filing down could be soldered to (with some effort and burning of my fingers), and screwed those into the ends of the copper pipe. To my surprise, the contact is relatively good!

Testing with small C0G capacitors, I can get a very neat resonant peak on the secondary loop. Adding a 0603 1Ohm resistor destroys it (but some resonance can still be seen), while two 1Ohm resistors in parallel remain certain resonance visible. This gives me hope that the Q is really high and will survive the electronic capacitor bank.

# (11-04-2026) First iteration of schematic design done

Got hyper-focus for the weekend and just did it all.

# (13-04-2026) Built a simple ArduinoISP

Did this on a breadboard, flashed the ArduinoISP sketch into a Nano and it works out of the box with `avrdude -c avrisp -P /dev/ttyUSB0 -b 19200 -p attiny85`
