"martin m1"
hsync_pulse_ms = 4.862
colour_gap_ms = 0.572
colour_time_ms = 146.342
samples_per_line = Fs*((colour_time_ms*3)+(colour_gap_ms*4) + hsync_pulse_ms)/1000.0

"scottie s1"
hsync_pulse_ms = 9
colour_gap_ms = 1.5
colour_time_ms = 138.240
samples_per_line = Fs*((colour_time_ms*3)+(colour_gap_ms*3) + hsync_pulse_ms)/1000.0

"martin m2"
hsync_pulse_ms = 4.862
colour_gap_ms = 0.572
colour_time_ms = 91.520
samples_per_line = Fs*((colour_time_ms*3)+(colour_gap_ms*4) + hsync_pulse_ms)/1000.0

"scottie s2"
hsync_pulse_ms = 9
colour_gap_ms = 1.5
colour_time_ms = 88.064
samples_per_line = Fs*((colour_time_ms*3)+(colour_gap_ms*3) + hsync_pulse_ms)/1000.0


        if (n - last_hsync_sample) > samples_per_line * 0.99 and (n - last_hsync_sample) < samples_per_line * 1.01:
          if confirmed_sync_sample is None:
            confirmed_sync_sample = n
          else:
            num_lines = round((n-confirmed_sync_sample)/samples_per_line)
            #mean_samples_per_line = mean_samples_per_line * 0.7 + ((n-confirmed_sync_sample)/num_lines) * 0.3
            #print(num_lines, n, last_hsync_sample, n-last_hsync_sample, samples_per_line, mean_samples_per_line)
        last_hsync_sample = n

      #colour pixels
      brightness = min(max(samples[n], 1500), 2300)
      brightness = 256*((brightness-1500)/(2300-1500))
      pixel_accumulator += brightness
      pixel_n += 1
      image_sample += 1


    n += 1
  plt.imshow(image)
  plt.show()

def sample_to_pixel(sample, mode, samples_per_line, samples_per_colour_line, samples_per_pixel, samples_per_hsync):

  """Use synchronous detection, from the sample number we can work out what the colour and x/y coordinates should be"""

  if mode == "martin":

    y = int(sample//samples_per_line)
    sample -= y*samples_per_line
    colour = int(sample//samples_per_colour_line)
    sample -= colour*samples_per_colour_line
    colour = [1, 2, 0, 3][colour] #martin colour order is g-b-r, map to r-g-b
    x = int(sample//samples_per_pixel)

  elif mode == "scottie":

    y = int(sample//samples_per_line)
    sample -= y*samples_per_line
    colour = int(sample//samples_per_colour_line)
    sample -= colour*samples_per_colour_line

    #hsync is between blue and red component (not at end of line)
    #for red component, subtract the length of the scan-line
    if colour == 2:
        sample -= samples_per_hsync
    if sample < 0:
        #return colour 3 for non-displayable pixels (e.g. during hsync)
        return 0, 0, 3

    colour = [1, 2, 0, 3][colour] #scottie colour order is g-b-r, map to r-g-b
    x = int(sample//samples_per_pixel)

  elif mode == "pd":

    y = int(sample//samples_per_line)
    sample -= y*samples_per_line

    colour = int(sample//samples_per_colour_line)
    sample -= colour*samples_per_colour_line
    colour = [1, 2, 0, 3][colour] #martin colour order is g-b-r, map to r-g-b
    x = int(sample//samples_per_pixel)

  return x, y, colour
    


decode(frequencies, Fs)

  

