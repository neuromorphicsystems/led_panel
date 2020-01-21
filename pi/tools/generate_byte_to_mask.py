bit_to_gpio = [20, 21, 26, 16, 19, 13, 6, 5];

print('static constexpr std::array<uint32_t, 256> _byte_to_mask = {', end = '')
for byte in range(0, 256):
    mask = 0
    for bit in range(0, 8):
        if ((byte >> bit) & 1) == 1:
            mask |= (1 << bit_to_gpio[bit])
    print('0b{0:032b}'.format(mask), end = ', ' if byte < 255 else '')
print('};')
