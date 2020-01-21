import led_panel
import numpy
import time

x = 0
y = 0
while True:
    frame = numpy.zeros((led_panel.height, led_panel.width), dtype=numpy.uint8)
    frame[y, x] = 255
    led_panel.send(brightness=50, packed_frame=led_panel.pack(frame))
    if x < led_panel.width - 1:
        x += 1
    else:
        x = 0
        if y < led_panel.height - 1:
            y += 1
        else:
            y = 0
