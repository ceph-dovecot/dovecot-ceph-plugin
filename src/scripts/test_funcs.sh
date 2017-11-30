#!/bin/bash

test_excluded_files() {
	declare -a str_array=("dovecot-acl-list" "dovecot.list.index.log" "dovecot-uidvalidity" "dovecot-uidvalidity.5a1401bc"\
		"subscriptions" "storage" "alt-storage" "dbox-Mails" "systemd" "mailboxes" "INBOX" "Trash")
	for str in "${str_array[@]}" ; do
		if is_excluded $str ; then
			echo "$str found" 
		else
			echo "$str not found"
		fi
	done
}

# main

script_path=${0%/*}
echo "script_path = $script_path"
source $script_path/utils.sh

test_environment

mail_location=$(get_user_mail_location $1)
if [ -z $mail_location ] ; then
	echo "mail_location not found!"
else
	echo "mail_location = $mail_location"
fi

test_excluded_files

result=$(doveadm_sync $2 $1)
[ $result -eq 0 ] && echo "success" || echo "fail"

copy_files $mail_location $2 $1 
