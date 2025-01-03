import imageio
import numpy as np
from matplotlib import pyplot as plt

input_file = "splash.png"

im = imageio.imread(input_file)
h, w, c = im.shape

class ToneGenerator:
  def __init__(self, Fs):
    self.Fs = Fs
    self.phase = 0
    self.sin_table = np.round(32767*np.sin(np.arange(1024)*2*np.pi/1024))
    self.frequencies = []
    self.tone = []
    self.residue = 0

  def get_sample(self, frequency):
    step = int((2**32)*frequency/self.Fs)
    self.phase += step
    self.phase &= 0xffffffff
    return self.sin_table[self.phase >> 22]

  def generate(self, frequency, time):
    samples_exact = self.Fs*time
    samples_exact += self.residue
    samples = int(samples_exact)
    self.residue = samples_exact-samples
    self.frequencies.extend([frequency for i in range(samples)])
    self.tone.extend([self.get_sample(frequency) for i in range(samples)])

  def save_tone(self, fname):
    data = np.array(self.tone, dtype="int16")
    with open(fname, "wb") as output_file:
      output_file.write(data.tobytes())

def calculate_parity(number):
    result = 0
    while number:
        result ^= number & 1
        number >>= 1
    return result

def generate_vis_bit(tone_generator, level):
    if level:
      tone_generator.generate(1100, 30e-3)
    else:
      tone_generator.generate(1300, 30e-3)

def generate_vis_code(tone_generator, mode, width, height):

  vis = 0

  if mode == "martin":
    vis |= 0x20

  elif mode == "scottie":
    vis |= 0x30

  else:
    assert False

  if width == 320:
    vis |= 0x4

  if height == 256:
    vis |= 0x8

  print(hex(vis))

  tone_generator.generate(1200, 30e-3)#start bit
  for i in range(7):
    generate_vis_bit(tone_generator, vis&1)
    vis >>= 1
  generate_vis_bit(tone_generator, calculate_parity(vis))
  tone_generator.generate(1200, 30e-3)#stop bit

def get_pixel(width, height, y, x, colour):
  y = int(y*h/height)
  x = int(x*w/width)
  pixel=im[y][x][colour] 
  pixel = 1500 + ((2300-1500)*pixel/256)
  return pixel
  

def generate_scottie(tone_generator, width, height):

  hsync_pulse_ms = 9
  colour_gap_ms = 1.5
  if width == 320:
    colour_time_ms = 138.240
  else:
    colour_time_ms = 88.064

  #send rows
  for row in range(height):
    print(row)
    tone_generator.generate(1500, colour_gap_ms * 1e-3)
    for col in range(width):
      tone_generator.generate(get_pixel(width, height, row, col, 1), colour_time_ms * 1e-3 / width)

    tone_generator.generate(1500, colour_gap_ms * 1e-3)
    for col in range(width):
      tone_generator.generate(get_pixel(width, height, row, col, 2), colour_time_ms * 1e-3 / width)

    tone_generator.generate(1200, hsync_pulse_ms * 1e-3)
    tone_generator.generate(1500, colour_gap_ms * 1e-3)
    for col in range(width):
      tone_generator.generate(get_pixel(width, height, row, col, 0), colour_time_ms * 1e-3 / width)


def generate_martin(tone_generator, width, height):

  hsync_pulse_ms = 4.862
  colour_gap_ms = 0.572
  if width == 320:
    colour_time_ms = 146.342
  else:
    colour_time_ms = 73.216

  #send rows
  for row in range(height):
    print(row)
    tone_generator.generate(1500, colour_gap_ms * 1e-3)
    for col in range(width):
      tone_generator.generate(get_pixel(width, height, row, col, 1), colour_time_ms * 1e-3 / width)

    tone_generator.generate(1500, colour_gap_ms * 1e-3)
    for col in range(width):
      tone_generator.generate(get_pixel(width, height, row, col, 2), colour_time_ms * 1e-3 / width)

    tone_generator.generate(1500, colour_gap_ms * 1e-3)
    for col in range(width):
      tone_generator.generate(get_pixel(width, height, row, col, 0), colour_time_ms * 1e-3 / width)

    tone_generator.generate(1500, colour_gap_ms * 1e-3)
    tone_generator.generate(1200, hsync_pulse_ms * 1e-3)
    

def generate_sstv(mode = "martin", width=320, height=256):

  hsync_pulse_ms = 4.862
  colour_gap_ms = 0.572
  colour_time_ms = 146.342

  tone_generator = ToneGenerator(15e3)

  tone_generator.generate(1900, 300e-3)
  tone_generator.generate(1200, 10e-3)
  tone_generator.generate(1900, 300e-3)
  generate_vis_code(tone_generator, mode, width, height)

  if mode == "martin":
    generate_martin(tone_generator, width, height)
  elif mode == "scottie":
    generate_scottie(tone_generator, width, height)
  else:
    assert False

  tone_generator.save_tone("test_tone.pcm")
  plt.plot(tone_generator.tone)
  plt.show()
  
  
generate_sstv("scottie", 320, 256)  
