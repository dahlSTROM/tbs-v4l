Howto
=====
rm -rf v4linstall && mkdir v4linstall && cd v4linstall

git clone https://github.com/bas-t/tbs-v4l.git && cd tbs-v4l

./configure --failsafe=no --adapters=16 --tbs=all

This will compile all supported tbs drivers using today's v4l branch and having done that, compile FFdecsawrapper against it.

If you want to compile only 6680 drivers, use --tbs=6680.

If you only want 6281/6285 drivers, just don't use the --tbs= option. It is the default behaviour.

If you compile without the --failsafe option, you will be using a known working v4l tree from june first 2014.

Currently supported: 6680, 6281, 6285

At the moment, 6281 and 6285 are in DVB-C mode only.

The --adapters=16 option is because I have 8 dvb-c adapters in my backend and I'm obviously going to use FFdecsawrapper.

If you have 10 adapters, use --adapters=20
