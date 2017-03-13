Dovecot RADOS Plugin
====================

## Configure, Compile, Install the plugin

To compile the plugin you need a configured or installed dovecot.

### Standard Installation in /usr/local
./autogen.sh

./configure

make

sudo make install

### User Installation in ~/dovecot
./autogen.sh

./configure --prefix=/home/user/dovecot

make install

### Configured Source Tree in ~/workspace/core
./autogen.sh

./configure --with-dovecot=/home/user/workspace/core

make install
