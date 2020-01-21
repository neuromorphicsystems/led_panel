import atexit
import os
import numpy
import subprocess

panels_width = 2
panels_height = 1

width = panels_width * 32
height = panels_height * 16

process = subprocess.Popen(
    args=[
        os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            'build',
            'led_panel_sink'),
        str(width // 32),
        str(height // 16)],
    stdin=subprocess.PIPE)

def kill_process():
    process.kill()
atexit.register(kill_process)

def send(brightness, packed_frame):
    """
    Sends a frame to the LED panel
    The display pixels must be grouped in bytes
    """
    assert type(brightness) is int
    assert brightness >= 0
    assert brightness < 256
    assert type(packed_frame) is numpy.ndarray
    assert packed_frame.dtype is numpy.dtype('uint8')
    assert packed_frame.size == width * height / 8
    process.stdin.write(bytes([brightness]))
    process.stdin.write(packed_frame.tobytes())
    process.stdin.flush()

def pack(frame):
    """
    Groups the pixels of a grey level frame in bytes
    """
    assert type(frame) is numpy.ndarray
    assert frame.dtype is numpy.dtype('uint8')
    assert frame.shape == (height, width)
    return numpy.packbits(frame // 128)
