import matplotlib.pyplot as plt
import numpy as np

ts = np.linspace(0, 0.01, 20_000_000)
n = len(ts)
dt = ts[1] - ts[0]
fcarrier0 = 200_000
fcarrier1 = 300_000
fck = 40_000

spb = round(2 / (fck*dt))
bits = np.repeat(np.random.randint(0,2, len(ts)//spb), spb)
# Trim ts so we don't have missized array issues
ts = ts[:len(bits)]

clock = (ts % (1/fck) < 1/(2*fck)).astype(int)
manchester = clock ^ bits


carrier0 = (ts % (1/fcarrier0) < 1/(2*fcarrier0)).astype(int)
carrier1 = (ts % (1/fcarrier1) < 1/(2*fcarrier1)).astype(int)
# carrier0 = np.sin(2 * np.pi * fcarrier0 * ts)
# carrier1 = np.sin(2 * np.pi * fcarrier1 * ts)

carrier = np.choose(manchester, [carrier0,carrier1])

freqs = np.fft.rfftfreq(len(carrier), dt)
fft = np.abs(np.fft.rfft(carrier))
fft = 20 * np.log10(fft / fft.max())


# fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True)
# ax1.set_xlim(0, 0.0001)
# ax1.plot(ts, manchester)
# ax2.plot(ts, carrier)
# plt.savefig("out.pdf")


fig, ax = plt.subplots(1,1)
ax.set_xlim(0, 1e6)
ax.set_ylim(-75, 0)
ax.plot(freqs, fft)
plt.savefig("out.pdf")

