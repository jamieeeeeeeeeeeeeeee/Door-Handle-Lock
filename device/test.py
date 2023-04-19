from PIL import Image
import socket

img = Image.open("asdasd.jpg")
img = img.resize((320, 240))

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.connect(("192.168.0.39", 9999))

for y in range(240):
    for x in range(320):
        r, g, b = img.getpixel((x, y))
        x1 = x
        x2 = 0
        if x1 > 255:
            x2 = x1 - 255
            x1 = 255
        server.send(bytes([r, g, b, x1, x2, y]))
