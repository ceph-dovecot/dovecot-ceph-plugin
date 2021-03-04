ldconfig
service dovecot.service start 

/usr/local/bin/imaptest user=t%d pass=t port=10143 users=100 clients=25 error_quit secs=15 output=/var/log/imaptest.log
smtp-source -v -L -s 1 -m 1 -c -F /root/lmtp_test_mail.tld -f test@example.com -t t1 inet:127.0.0.1:1024
doveadm -D altmove -u t2 ALL
doveadm -D altmove -r -u t2 ALL

doveadm -Dv backup -u t1 -m INBOX mdbox:/usr/local/var/mail/mdbox/t1
doveadm -D fetch -u t1 "guid date.received date.sent flags pop3.uidl seq size.virtual uid user mailbox-guid mailbox" ALL > /root/rbox.t1.mails
doveadm -D -c /usr/local/etc/dovecot_mdbox/dovecot.conf fetch -u t1 "guid date.received date.sent flags pop3.uidl seq size.virtual uid user mailbox-guid mailbox" ALL > /root/mdbox.t1.mails
repo/src/scripts/sort.sh /root/rbox.t1.mails /root/rbox.t1.mails.sorted
repo/src/scripts/sort.sh /root/mdbox.t1.mails /root/mdbox.t1.mails.sorted

diff -y -W 1500 /root/rbox.t1.mails.sorted /root/mdbox.t1.mails.sorted
rm -r /usr/local/var/mail/rbox/t1
doveadm -Dv -c /usr/local/etc/dovecot_mdbox/dovecot.conf backup -u t1 -m INBOX rbox:/usr/local/var/mail/rbox/t1
doveadm -D fetch -u t1 "guid date.received date.sent flags pop3.uidl seq size.virtual uid user mailbox-guid mailbox" ALL > /root/rbox.t1.mails
/repo/src/scripts/sort.sh /root/rbox.t1.mails /root/rbox.t1.mails.sorted
diff -y -W 1500 /root/rbox.t1.mails.sorted /root/mdbox.t1.mails.sorted

rm -r /usr/local/var/mail/rbox
/usr/local/bin/imaptest user=t%d pass=t port=10110 profile=/root/pop3-profile.conf users=100 clients=10 error_quit secs=15 output=/var/log/imaptest.log

rm -r /usr/local/var/mail/rbox
/usr/local/bin/imaptest user=t%d pass=t port=10143 error_quit secs=15 copybox=INBOX.Drafts output=/var/log/imaptest.log
doveadm -D force-resync -u t1 INBOX
/usr/local/bin/exec.sh "cat /var/log/dovecot.log | grep \"Error:\""
/usr/local/bin/exec.sh "cat /var/log/dovecot.log | grep \"failed:\""
/usr/local/bin/exec.sh "cat /var/log/dovecot.log | grep \"Internal error\""
/usr/local/bin/exec.sh "cat /var/log/dovecot.log | grep \"killed\""
/usr/local/bin/exec.sh "cat /var/log/dovecot.log | grep \"Panic:\""
/usr/local/bin/exec.sh "cat /var/log/dovecot.log | grep \"Fatal:\""
/usr/local/bin/exec.sh "cat /var/log/imaptest.log | grep \"BUG:\""

service dovecot.service stop
