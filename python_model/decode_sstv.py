import numpy as np
from scipy.signal import hilbert
from matplotlib import pyplot as plt

dt = np.dtype("int16")
dt = dt.newbyteorder('<')
Fs = 15000

last = 0
#with open("SSTV_sunset_audio.wav" , "rb") as inf:
#with open("test.wav" , "rb") as inf:
#with open("../test_files/test.wav" , "rb") as inf:
with open("../test_files/PD90.wav" , "rb") as inf:
#with open("test_45.wav" , "rb") as inf:
#with open("test_1_35.wav" , "rb") as inf:

  buffer = inf.read(1<<22)
  array = np.frombuffer(buffer, dtype=dt)

array = np.array(array)

array = hilbert(array)
phases = np.angle(array)
frequencies = (phases[1:]-phases[:-1]) % (np.pi)
frequencies *= Fs/(2*np.pi)

smoothed_data = 0
for n in range(len(frequencies)):
  smoothed_data = smoothed_data*0.93 + frequencies[n]*0.07
  frequencies[n] = smoothed_data


#plt.plot(array.real)
#plt.plot(array.imag)
#plt.show()

plt.plot(np.arange(len(frequencies))/Fs, frequencies)
plt.ylim(1100, 2300)
plt.show()

modes = {}

"martin m1"
width = 320
hsync_pulse_ms = 4.862
colour_gap_ms = 0.572
colour_time_ms = 146.342
samples_per_line = Fs*((colour_time_ms*3)+(colour_gap_ms*4) + hsync_pulse_ms)/1000.0
samples_per_colour_line = Fs*(colour_time_ms+colour_gap_ms)/1000.0
samples_per_colour_gap = Fs*colour_gap_ms/1000.0
samples_per_pixel = samples_per_colour_line/width
samples_per_hsync = Fs*hsync_pulse_ms/1000.0

modes["martin_m1"] = (width, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync, samples_per_colour_gap)

"scottie s1"
width = 320
hsync_pulse_ms = 9
colour_gap_ms = 1.5
colour_time_ms = 138.240
samples_per_line = Fs*((colour_time_ms*3)+(colour_gap_ms*3) + hsync_pulse_ms)/1000.0
samples_per_colour_line = Fs*(colour_time_ms+colour_gap_ms)/1000.0
samples_per_colour_gap = Fs*colour_gap_ms/1000.0
samples_per_pixel = samples_per_colour_line/width
samples_per_hsync = Fs*hsync_pulse_ms/1000.0
modes["scottie_s1"] = (width, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync, samples_per_colour_gap)

"martin m2"
width = 160
hsync_pulse_ms = 4.862
colour_gap_ms = 0.572
colour_time_ms = 73.216
samples_per_line = Fs*((colour_time_ms*3)+(colour_gap_ms*4) + hsync_pulse_ms)/1000.0
samples_per_colour_line = Fs*(colour_time_ms+colour_gap_ms)/1000.0
samples_per_colour_gap = Fs*colour_gap_ms/1000.0
samples_per_pixel = samples_per_colour_line/width
samples_per_hsync = Fs*hsync_pulse_ms/1000.0
modes["martin_m2"] = (width, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync, samples_per_colour_gap)

"scottie s2"
width = 160
hsync_pulse_ms = 9
colour_gap_ms = 1.5
colour_time_ms = 88.064
samples_per_line = Fs*((colour_time_ms*3)+(colour_gap_ms*3) + hsync_pulse_ms)/1000.0
samples_per_colour_line = Fs*(colour_time_ms+colour_gap_ms)/1000.0
samples_per_colour_gap = Fs*colour_gap_ms/1000.0
samples_per_pixel = samples_per_colour_line/width
samples_per_hsync = Fs*hsync_pulse_ms/1000.0
modes["scottie_s2"] = (width, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync, samples_per_colour_gap)

"pd 50"
width = 320
hsync_pulse_ms = 20
colour_gap_ms = 2.08
colour_time_ms = 91.520
samples_per_line = Fs*((colour_time_ms*4)+(colour_gap_ms*1) + hsync_pulse_ms)/1000.0
samples_per_colour_line = Fs*(colour_time_ms)/1000.0
samples_per_colour_gap = Fs*colour_gap_ms/1000.0
samples_per_pixel = samples_per_colour_line/width
samples_per_hsync = Fs*hsync_pulse_ms/1000.0
modes["pd_50"] = (width, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync, samples_per_colour_gap)

"pd 90"
width = 320
hsync_pulse_ms = 20
colour_gap_ms = 2.08
colour_time_ms = 170.240
samples_per_line = Fs*((colour_time_ms*4)+(colour_gap_ms*1) + hsync_pulse_ms)/1000.0
samples_per_colour_line = Fs*(colour_time_ms)/1000.0
samples_per_colour_gap = Fs*colour_gap_ms/1000.0
samples_per_pixel = samples_per_colour_line/width
samples_per_hsync = Fs*hsync_pulse_ms/1000.0
modes["pd_90"] = (width, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync, samples_per_colour_gap)

"SC2120"
width = 320
hsync_pulse_ms = 5
colour_gap_ms = 0
colour_time_ms = 117
samples_per_line = Fs*((colour_time_ms*4) + hsync_pulse_ms)/1000.0
samples_per_colour_line = Fs*(colour_time_ms)/1000.0
samples_per_colour_gap = Fs*colour_gap_ms/1000.0
samples_per_pixel = samples_per_colour_line/width
samples_per_hsync = Fs*hsync_pulse_ms/1000.0
modes["sc2_120"] = (width, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync, samples_per_colour_gap)

def decode(samples, Fs):
  vsync_samples = int(Fs*300/1000) 
  vsync_gap_samples = int(Fs*10/1000) 
  samples_per_bit = int(Fs*30/1000) 

  #default values get replaced when vis is decoded
  height = 256
  width = 320
  decode_mode = "scottie_m2"

  n = 0
  sync_counter = 0
  y_pixel = 0
  sync_state = "detect"
  state = "detect_sync"
  image = np.zeros([height+10, width+10, 3], dtype="int")
  plot_data = []
  smoothed_sample = 0
  last_hsync_sample= 0
  confirmed_sync_sample = None
  line_length = 0
  first_sync = 0

  while 1:

    #detect scan syncs
    sync_found = False
    if sync_state == "detect":

      if samples[n] < 1300 and samples[n-1] > 1300:
        sync_state = "sync_found"
        sync_counter = 0

    elif sync_state == "sync_found":

      if samples[n] < 1300:
        sync_counter += 1
      elif sync_counter > 0:
        sync_counter -= 1

      if sync_counter == 5:
        sync_found = True
        line_length = n-last_hsync_sample
        last_hsync_sample = n
        sync_state = "detect"

    #if sync_found:
      #print(line_length)


    if state == "detect_sync":
      if sync_found:
        for mode in modes.keys():
          if line_length > 0.99*modes[mode][1] and line_length < 1.01*modes[mode][1]:
            (width, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync, samples_per_colour_gap) = modes[mode]
            mean_samples_per_line = samples_per_line
            decode_mode = mode
            timeout = samples_per_line
            confirm_count = 0
            state = "confirm_sync"

    elif state == "confirm_sync":
      if sync_found:
        if line_length > 0.99*modes[decode_mode][1] and line_length < 1.01*modes[decode_mode][1]:
          state = "wait_rising_edge"
          first_sync = n
        else:
          confirm_count += 1
          if confirm_count == 2:
            state = "detect_sync"

    elif state == "wait_rising_edge":
      print(decode_mode)
      state = "decode_line"
      pixel_accumulator = 0
      pixel_n = 0
      last_x = 0
      image_sample = 0

    #1200Hz for n ms
    elif state == "decode_line":

      x, y, colour = sample_to_pixel(image_sample, decode_mode, mean_samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync)

      if x != last_x:
        if colour < 3:
          image[y][x][colour] = int(pixel_accumulator//pixel_n)
          pixel_accumulator = 0
          pixel_n = 0
          last_x = x

      #end of image
      if y == 256:
        state = "detect_sync"
        sync_counter = 0
        break

      #slant detection
      if sync_found:
        print(line_length)
        #confirm sync if close to expected time
        if line_length > samples_per_line * 0.99 and line_length < samples_per_line * 1.01:
          timeout = 10000
          num_lines = round((n - first_sync)/samples_per_line)
          #adjust line length
          mean_samples_per_line = mean_samples_per_line * 0.7 + ((n-first_sync)/num_lines) * 0.3
        else:
          timeout -= 1
          if timeout == 0:
            state = "detect_sync"
            sync_counter = 0
            break

      #colour pixels
      brightness = min(max(samples[n], 1500), 2300)
      brightness = 256*((brightness-1500)/(2300-1500))
      pixel_accumulator += brightness
      pixel_n += 1
      image_sample += 1


    n += 1
    if n==len(samples):
      break

  if decode_mode.startswith("pd"):
    for row in range(256):
      for col in range(320):
        y = image[row][col][0]
        cr = image[row][col][1]
        cb = image[row][col][2]

        cr = cr - 128
        cb = cb - 128
        r = y + 45 * cr / 32
        g = y - (11 * cb + 23 * cr) / 32
        b = y + 113 * cb / 64

        image[row][col][0] = r
        image[row][col][1] = g
        image[row][col][2] = b

  plt.imshow(image)
  plt.show()

def sample_to_pixel(sample, decode_mode, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync):

  """Use synchronous detection, from the sample number we can work out what the colour and x/y coordinates should be"""

  if decode_mode == "martin_m1" or decode_mode == "martin_m2":

    y = int(sample//samples_per_line)
    sample -= y*samples_per_line
    colour = int(sample//samples_per_colour_line)
    sample -= colour*samples_per_colour_line
    colour = [1, 2, 0, 3][colour] #martin colour order is g-b-r, map to r-g-b
    x = int(sample//samples_per_pixel)

  elif decode_mode == "scottie_s1" or decode_mode == "scottie_s2":

    #with scottie, sync id mid-line between blue and red.
    #subtract the red period to sync to next full line
    sample -= samples_per_colour_line
    sample -= samples_per_hsync
    if sample < 0:
      return 0, 0, 3

    y = int(sample//samples_per_line)
    sample -= y*samples_per_line

    #hsync is between blue and red component (not at end of line)
    #for red component, subtract the length of the scan-line
    if sample < 2*samples_per_colour_line:
      colour = int(sample//samples_per_colour_line)
      sample -= colour*samples_per_colour_line
    else:
      sample -= 2*samples_per_colour_line
      sample -= samples_per_hsync
      colour = 2 + int(sample//samples_per_colour_line)

    if sample < 0:
        #return colour 3 for non-displayable pixels (e.g. during hsync)
        return 0, 0, 3

    colour = [1, 2, 0, 3][colour] #scottie colour order is g-b-r, map to r-g-b
    x = int(sample//samples_per_pixel)

  elif decode_mode == "pd_50" or decode_mode == "pd_90":

    sample -= samples_per_hsync
    y = int(sample//samples_per_line)
    sample -= y*samples_per_line
    colour = int(sample//samples_per_colour_line)
    sample -= colour*samples_per_colour_line
    colour = [0, 1, 2, 3, 3][colour] 
    x = int(sample//samples_per_pixel)

  elif decode_mode.startswith("sc2"):

    y = int(sample//samples_per_line)
    sample -= y*samples_per_line


    #for red component, subtract the length of the scan-line
    if sample < samples_per_colour_line:
      colour = 0
      x = int(sample//samples_per_pixel)
    elif sample < 3*samples_per_colour_line:
      colour = 1
      sample -= samples_per_colour_line
      x = int(sample//(2*samples_per_pixel))
    elif sample < 4*samples_per_colour_line:
      colour = 2
      sample -= 3*samples_per_colour_line
      x = int(sample//samples_per_pixel)
    else:
      colour = 3
      x = 0

    if sample < 0:
        #return colour 3 for non-displayable pixels (e.g. during hsync)
        return 0, 0, 3

  return x, y, colour

    


decode(frequencies, Fs)

  

