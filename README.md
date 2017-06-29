Dovecot RADOS Plugins
=====================

The goal of this project is the creation of a storage plugin for Dovecot, which enables the storage of emails in Ceph RADOS objects. The focus is currently on a hybrid model where the emails are stored in RADOS objects, while all other metadata (lists, index, cache) are stored in a file system that is located locally on the Dovecot server or on shared CephFS volumes. The latter allows the operation of Dovecot completely on Ceph.

As a bonus, a dictionary plugin is included, which allows the storage of Dovecot dictionaries in Ceph OMAPs.

## The Hybrid Storage Model

The mails are saved directly as RADOS objects. All other data are stored as before in the file system. This applies in particular to the data of the lib-index of Dovecot. We assume the file system is designed as shared storage based on CephFS.

Based on the code of the Dovecot storage format [Cydir](http://wiki.dovecot.org/MailboxFormat/Cydir) we developed a hybrid storage as Dovecot plugin. The hybrid storage directly uses the librados for storing mails in Ceph objects. The mail objects are immutable and get tored in one RADOS object.  Immutable metadata is stored in omap KV and xattr. The index data is completely managed by the lib-index and ends up in CephFS volumes.

Because of the way MUAs access the mails, it may be necessary to provide a local cache of mails from Dovecot. The cache can be located in the main memory or on local SSD storage. However, this optimization is optional and will be implemented only if necessary.

![Overview](doc/images/dovecot-ceph-hybrid-libindex-rmb-cache.png)

The mail objects and CephFS should be placed in different pools. The mail objects are immutable and require a lot of storage. They would benefit a lot from [erasure coded pools](http://docs.ceph.com/docs/master/architecture/#erasure-coding). The index data required a lot of writing and are placed on an SSD based CephFS pool.

## Compile and install the Plugins

To compile the plugin you need a configured or installed dovecot.

### Standard installation in /usr/local

    ./autogen.sh
    ./configure
    make
    sudo make install

### User installation in ~/dovecot

    ./autogen.sh
    ./configure --prefix=/home/user/dovecot   
    make install

### Configured source tree in ~/workspace/core

    ./autogen.sh   
    ./configure --with-dovecot=/home/user/workspace/core 
    make install

## Testing the Plugins

The source sirectories contain test applications named test-*. They use the configuration in the same directory. The configurations assumes a Ceph cluster running locally without _cephx_, that has for example been created using _vstart.sh_ as decribed in [DEVELOPER GUIDE (QUICK)](http://docs.ceph.com/docs/master/dev/quick_guide/) or [ceph/README.md](https://github.com/ceph/ceph/blob/master/README.md). 

    ../src/vstart.sh -d -X -n -l 

Any other way to get a Ceph cluster is valid, too.

## Configure the plugins

Both plugins use the default Ceph configuration way described in [Step 2: Configuring a Cluster Handle](http://docs.ceph.com/docs/master/rados/api/librados-intro/#step-2-configuring-a-cluster-handle):

1. `rados_conf_parse_env()`: Evaluate the CEPH_ARGS environment variable.
2. `rados_conf_read_file()`: Search the default locations, and the first found is used. The locations are:
   * $CEPH_CONF (environment variable)
   * /etc/ceph/ceph.conf
   * ~/.ceph/config
   * ceph.conf (in the current working directory)

### Storage RADOS plugin

To load the plugin add _storage_rbox_ to the list of mail plugins. The name of the mailbox format is `rbox`. Add the plugin to `10-mail.conf` as _mail_location_. See [Mail location](http://wiki.dovecot.org/MailLocation) for details. 

    mail_location = rbox:/home/user/dovecot/var/mail/rados/%u

The plugin uses _mail_storage_ as pool name. If the pool is missing, it will be created. This will become configurable soon.

### Dict RADOS plugin

To load the plugin add _dict_rados_ to the list of mail plugins. The name of the dict driver is `rados`. Add the plugin for example to `10-mail.conf` as _mail\_attribute\_dict_. See [Dovecot Dictionaries](http://wiki.dovecot.org/Dictionary) for details.  

    mail_plugins = dict_rados 
    mail_attribute_dict = rados:oid=metadata:pool=mail_dictionary

The configuration parameters are:

* **oid**: The RADOS object id to use. 
* **pool**: The RADOS pool to use for the dictionary objects. The default pool name is _mail_dictionary_. If the pool is missing, it will be created.

All key/values will be stored in OMAP key/values of the object <oid>.

## Thanks

This plugin borrows heavily from Dovecot itself particularly for the automatic detection of dovecont-config (see m4/dovecot.m4). The lib-dict and lib-storage were also used as reference material for understanding the Dovecot dictionary and storage API.
