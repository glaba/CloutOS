from PIL import Image
import progressbar
from time import sleep
import sys

print "USAGE: first arg: image filename    second arg: name of .h file to create"
name = sys.argv[2]

im = Image.open(sys.argv[1]) # Can be many different formats.
pix = im.load()
bar = progressbar.ProgressBar(maxval = 768*1024, widgets=[progressbar.Bar('=', '[', ']'), ' ', progressbar.Percentage()])
f = open(name + ".h", "w")
bar.start()
pixel = "#ifndef _" + name.upper() + "_H\n#define _" + name.upper() + "_H\n"

pixel += "uint32_t " + name + "[1024 * 768] = { \n"

for y in range(768):
	for x in range(1024):
		pixel += '0x'
		pixel += '{:02x}'.format(pix[x, y][0])
		pixel += '{:02x}'.format(pix[x, y][1])
		pixel += '{:02x}'.format(pix[x, y][2])
		pixel += ', '
		bar.update(x * y)
pixel += "\n}; \n"
pixel += "#endif \n\n"
f.write(pixel)
f.close()
bar.finish()
