docker stop ceph_dovecot_build ceph_cluster_demo
docker rm ceph_dovecot_build ceph_cluster_demo
docker volume rm ceph_config_test
docker network rm ceph_config_net
docker volume create --name ceph_config_test
docker network create --driver=bridge --subnet=192.168.204.0/24 --gateway=192.168.204.1 ceph_config_net
docker run -d --net=ceph_config_net --name ceph_cluster_demo --mount type=tmpfs,destination=/etc/ceph -v ceph_config_test:/root/cfg -e MON_IP=192.168.204.2 -e CEPH_PUBLIC_NETWORK=192.168.204.0/24 -e CEPH_DEMO_UID=test_uuid ceph/daemon:latest demo
docker run -itd --net=ceph_config_net --name ceph_dovecot_build  -e SOURCE_VERSION=2.3.15 -v ceph_config_test:/etc/ceph -v $(pwd):/repo cephdovecot/travis-build-master-2.3:latest sh


docker exec ceph_dovecot_build sh -c 'printf "nameserver 8.8.8.8\n" > /etc/resolv.conf'
docker exec ceph_dovecot_build sh -c 'rm /etc/apt/sources.list.d/*'
docker exec ceph_dovecot_build sh -c 'add-apt-repository ppa:git-core/ppa -y'
docker exec ceph_dovecot_build sh -c '(DEBIAN_FRONTEND=noninteractive apt update & apt-get install -qq -y flex bison git)'
docker exec ceph_dovecot_build sh -c 'cd /usr/local/src/dovecot; git fetch origin'
docker exec ceph_dovecot_build sh -c 'cd /usr/local/src/dovecot; git checkout 2.3.15'
docker exec ceph_dovecot_build sh -c 'cd /usr/local/src/dovecot; ./autogen.sh && ./configure --enable-maintainer-mode --enable-devel-checks --with-zlib'
docker exec ceph_dovecot_build sh -c 'cd /usr/local/src/dovecot; make install'
#docker exec ceph_dovecot_build sh -c './autogen.sh && ./configure --with-dovecot=/usr/local/lib/dovecot --enable-maintainer-mode --enable-debug --with-integration-tests --enable-valgrind --enable-debug'

docker exec ceph_cluster_demo sh -c 'cp -r /etc/ceph/* /root/cfg'
docker exec ceph_dovecot_build sh -c 'chmod 777 /etc/ceph/*'
docker exec ceph_cluster_demo sh -c 'ceph tell mon.\* injectargs "--mon-allow-pool-delete=true"'

