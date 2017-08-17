./autogen.sh
#export CXXFLAGS='-O2 -fPIC -g -m64 -fmessage-length=0 -D_FORTIFY_SOURCE=2 -fstack-protector -funwind-tables -fasynchronous-unwind-tables -I/usr/lib64'
#./configure --prefix=/usr --includedir=/usr/include --libdir=/usr/lib64 --libexecdir=/usr/lib --with-dovecot=/usr/lib64/dovecot
./configure --prefix=/usr --with-dovecot=/usr/lib64/dovecot
#cd src/librmb
#make
#sudo make install
#cd ../..
make
sudo make install
