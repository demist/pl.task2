CFLAGS="-std=c99 -I/usr/include/lua5.2/" LDFLAGS="-L/usr/lib/x86_64-linux-gnu/ -llua5.2" python3.4 setup.py build
python3.4 setup.py install
