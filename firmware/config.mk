# Serial port for CCTL
#
# For CYGWIN (note ttyS0 = COM1)
# CCTL_DEVICE=/dev/ttyS6    # COM7
#
# For OSX
# CCTL_DEVICE=/dev/tty.usbserial-FTE9OZ7V
#
# For Linux
CCTL_DEVICE ?= /dev/ttyACM0
#$(error Edit config.mk for your serial port location, then comment out this line)

# Device type
#$(error Edit config.mk for your device type then comment out this line)
# uncomment line for Ciseco XRF v1.5+/CC1111 (24MHz)
# CRYSTAL_24_MHZ=Y
# uncomment line for Ciseco XRF v1.4 and lower (26MHz)
CRYSTAL_26_MHZ=Y

CONFIG = \
    --eui64=0000000000000003 \
    --mac=02AADEADBEEF \
    --server=sta.kri.dk \
    --port=6379 \
    --feedid=MYFEEDID \
    --apikey=MYPACHUBEAPIKEY\
    --keyenc=250B7847A2BB41856DD41B4017A9036A \
    --keymac=174AA251FFADDC276EA364EDA62EDFDF

