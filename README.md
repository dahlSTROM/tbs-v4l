Howto
=====

git clone https://github.com/bas-t/tbs-v4l.git && cd tbs-v4l

./configure --adapters=16

This will compile supported tbs drivers using today's v4l branch and having done that, compile FFdecsawrapper against it.

Currently supported: 6680

The --adapters=16 option is because I have 8 dvb-c adapters in my backend and I'm obviously going to use FFdecsawrapper.

If you have 10 adapters, use --adapters=20
