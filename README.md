Dovecot RADOS Plugin
====================

## Configure, Compile, Install the plugin

### Standard Installation in /usr/local
./autogen.sh

./configure

make

sudo make install

### User Installation in ~/dovecot
./autogen.sh

./configure --prefix=/home/user/dovecot

make install
