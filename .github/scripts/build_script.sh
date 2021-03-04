cd /usr/local/src/dovecot
make install

cd /repo/dovecot-ceph-plugin/dovecot-ceph-plugin/
./autogen.sh
./configure --with-dovecot=/usr/local/lib/dovecot --enable-maintainer-mode --enable-debug --with-integration-tests --enable-valgrind --enable-debug
make clean install

chmod 777 /etc/ceph/*

ldconfig

chmod -R 777 /usr/local/var/
#service dovecot.service start
dovecot
