#!/bin/bash

test_excluded_files() {
	declare -a str_array=("dovecot-acl-list" "dovecot.list.index.log" "dovecot-uidvalidity" "dovecot-uidvalidity.5a1401bc"\
		"subscriptions" "storage" "alt-storage" "dbox-Mails" "rbox-Mails" "systemd" "mailboxes" "INBOX" "Trash")
	for str in "${str_array[@]}" ; do
		if is_excluded "$str" ; then
			echo "$str found" 
		else
			echo "$str not found"
		fi
	done
}

# main

script_path=${0%/*}
echo "script_path = $script_path"
source "$script_path/utils.sh"
source "$script_path/doveadm.sh"

test_environment

mail_location=$(get_user_mail_location "$1")
if [ -z "$mail_location" ] ; then
	echo "mail_location not found!"
else
	echo "mail_location = $mail_location"
fi

#IFS=$':'; ml_array=($mail_location); unset IFS;
#for ml in "${ml_array[@]}"; do
	#echo "$ml"
#done	


#test_excluded_files

#result=$(doveadm_sync $2 $1 $3)
#[ $result -eq 0 ] && echo "success" || echo "fail: $result"

#copy_files $mail_location $2 $1 
 
list=$(get_mailbox_list "$1")
sep='/'
IFS=$'\n'; mbox_array=($list); unset IFS;
for mbox in "${mbox_array[@]}"; do
	if ! string_contains "$mbox" "$sep"; then
		echo " - $mbox"
	fi
done

#result=$(delete_mailbox t34 dovecot)
#[ "$result" -eq 0 ] && echo "success" || echo "fail: $result"
