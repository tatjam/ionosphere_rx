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

# (17-04-2026) Proof of concept on breadboards

Turns out when desoldering the programming pin from my Nano I somehow decoupled the ground! So I had to add a jumper from one side to another of the arduino board... Also forgot to disable the clock prescaler, so F_CPU was actually 1MHz. Now, with it set to 8MHz, it's able to send commands to the DAC at a rate of 10kHz.

Very important, the x2 output amplifier on the DAC clamps the upper voltages in a non-deterministic manner, so design the varactor driver op-amp to amplify by x2 additionally.


# (18-04-2026) PLL experiments

The communication protocol will use simple Manchester encoding:

## Transmitter

The transmitter simply runs a clock, and feeds the data at half the clock-rate XORed with the clock (IEEE 802.3 standard).

## Receiver

The receiver has an interrupt for a pin change, a timer, a "T" variable, and a "idle" state.

From an [IDLE] state, the receiver detects a pin change interrupt. This moves the state machine to [RUN] state and starts a up-counting timer. The T variable is set to its maximum value.

From the [RUN] state with counter > 1.5T, interrupts are ignored.

From the [RUN] state with counter < 1.5T its max value, interrupts are handled and data is received (0->1 = 1, 1 -> 0 = 0). Furthermore, the counter is restarted to the value adjusted to 3T', where T := 2T-C (or simply C if T is the maximum value), to correct for clock drift. Note that in this scheme:
- If clock expired below T, T' is set higher. Thus, the local clock is running faster than real clock and we need to slow down.
- If clock expired above T, T' is set lower. Thus, the local clock is running slower than real clock and we need to speed up.

If on the [RUN] state the timer expires, we return to [IDLE] state.

We operate on 8bit timer for efficiency, thus to prevent wrap around we need 3T's representation reasonably below 255, say we expect it to range [190, 230].

We will consider our "high performance" target to be packets of 16 bits sent atleast 1000 times per second, thus 16000bits per second. This will allow tuning the antenna 1000 times per second which seems to be more than enough.

### 20MHz operation

If we clock the 8-bit timer at 20MHz, and pre-scale by 8, we can have expiries as 3T goes from [190, 230] of [76, 92]us. Thus, T = [25, 31]us, and we have a clock rate of nominally (middle value = 28us) 1/T = 35.71kHz, allowing up to 17855 bits per second, or nearly 3kB/s.

We have nearly 1120 cycles per bit.

Alternatively, if we don't pre-scale the clock, we have expiries as 3T goes from [190, 230] of [9.5, 11.5]us. Thus, T = [3.16, 3.833]us, and we have a clock rate of nominally (middle value = 7us) 1/T = 142.9kHz, allowing up to 71450 bits per second, or nearly 9kB/s.

This last option is probably a bit tight for the core running at 20MHz as we have almost 280 cycles per bit.

### 8MHz operation

Same as before, if we pre-scale by 8, we have expiries as 3T goes from [190, 230] of [190, 230]us, and we have a clock rate of nominally (middle value = 210us) 4.76kHz, allowing up to 2380 bits per second, or nearly 300B/s.

We have nearly 3361 cycles per bit.

Alternatively, if we don't pre-scale the clock, we have expiries as 3T goes from [190, 230] of [23.75, 28.75]us. Thus, T = [7.91, 9.58]us, and we have a clock rate of nominally (middle value = 17.5us) 1/T = 57.14kHz, allowing up to 28570 bits per second, or nearly 3.6kB/s.

This last option is probably a bit tight for the core running at 8MHz as we have 200 cycles per bit.

### Decision

Because the 20MHz, pre-scaler 8 is appropriate for our "production" use case, we won't use a 16-bit timer. For development purposes, the 8MHz operation is good enough if only a bit slow.

# (19-04-2026) PLL Experiments

Forgot the INH pulldown resistor! Now, with it, the PLL works.

At a voltage of VCO=4.72V, C = 1nF, R1 = 22k I'm seeing a frequency of 4.4MHz, very jittery
At a voltage of VCO=4.5V, C = 1nF, R1 = 22k I'm seeing a frequency of 4.4MHz, very jittery
At a voltage of VCO=4.2V, C = 1nF, R1 = 22k I'm seeing a frequency of 352kHz
At a voltage of VCO=4.0V, C = 1nF, R1 = 22k I'm seeing a frequency of 327kHz.
At a voltage of VCO=3.0V, C = 1nF, R1 = 22k I'm seeing a frequency of 231kHz.
At a voltage of VCO=2.6V, C = 1nF, R1 = 22k I'm seeing a frequency of 190kHz.
At a voltage of VCO=2.0V, C = 1nF, R1 = 22k I'm seeing a frequency of 149.6kHz.
At a voltage of VCO=1.0V, C = 1nF, R1 = 22k I'm seeing a frequency of 74.3kHz.
At a voltage of VCO=0.8V, C = 1nF, R1 = 22k I'm seeing no oscillation.
At a voltage of VCO=0.5V, C = 1nF, R1 = 22k I'm seeing no oscillation.

Output frequency is slightly jittery, despite DAC output being nearly perfect.

Nwo, the chosen frequency of {180, 275}kHz is too low if we want a baseband bandwidth of 32kHz (or higher) and to use a simple RC low pass, we will need a stepper filter, or to use a slightly higher carrier frequency. I'll experiment now with {370, 500kHz} carrier.

# (20-04-2026) Proper PLL design

I have decided that the EEPROM will not be used and each antenna will be directly given the 12 bit DAC order. If the EEPROM is to be added back in the future, a MCU with more pins may be used!

- I may add a loop lock pin just in case, which is possible due to having free pins now

# (27-04-2026) Proper PLL design (2)

- Out 35.7kHz clock, for 5x bandwidth, requires 178.5kHz bandwidth
- Regardless, lets assume we want a 180kHz real bandwidth for the VCO voltage
- We can tolerate
- A simple lead-lag filter (see datasheet, offset one) with
	- Bandpass frequency of 180kHz
	- 0.1 (-20dB) of attenuation at stop-band
- This filter has a stop-band frequency of 2MHz, which is too high
- But we can just filter the buffered output afterwards to get a slightly better signal, a simple

# (25-05-2026) HV source oscillator build

Built on a breadboard, was very very unstable until I tied the gates physically next to the ones used to ground, at which point it worked!
With component more or less similar to design, oscillation at ~6kHz (or maybe 8, I don't remember fully, should have taken screenshot!)

Measuring on the breadboard 5V supply, maybe a 25mV or so noise was seen at the oscillation frequency. Need to add significant filtering to all the power supplies of oscillating stuff:

- The oscillator itself is okay with the 100nF decoupling
- Ferrite on supply line to mosfet driver section
- Optional gate resistor, DNP or 10 / 20 ohms to smooth rise time and reduce noise
