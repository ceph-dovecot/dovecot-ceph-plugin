Libsodium password hashing schemes plugin
=========================================

Requires installed libsodium: https://download.libsodium.org/doc/installation/


**Configure, Compile, Install the plugin:**

./autogen.sh

./configure

make

sudo make install


**Test the plugin:**

doveadm pw -s scrypt

doveadm pw -s argon2
