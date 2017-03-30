Dovecot RADOS Plugins
=====================

## Compile, Install the Plugins

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

## Configure the Plugins

### Dict RADOS Plugin

The name of the dict driver is `rados`. Add the plugin for example to 10-mail.conf as mail\_attribute\_dict. See [Mail location](http://wiki.dovecot.org/Dictionary) for details.  

    mail_attribute_dict = rados:oid=metadata:pool=librmb-index:config=/home/user/dovecot/etc/ceph/ceph.conf

The configuration parameters are:

* **oid**: The RADOS object id to use. 
* **pool**: The RADOS poll to use for the dictionary objects. 
* **config**: Absolut path to a Ceph configuration file.

The mail users username will be used as Ceph namespace. All key/values will be stored in OMAP Key/values of the object <oid>.

Shared key/values are not supported rigeht now.

### Storage RADOS Plugin

The name of the mailbox format is `rados`. Add the plugin to 10-mail.conf as mail_location. See [Mail location](http://wiki.dovecot.org/MailLocation) for details. 

    mail_location = rados:/home/user/dovecot/var/mail/rados/%u

