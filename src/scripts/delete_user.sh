#!/bin/bash

# main

# Loads utils script
script_path=${0%/*}
source $script_path/utils.sh

# Reads mail_location of user
mail_location=$(get_user_mail_location $1)

# Get all mailboxes
list=$(get_mailbox_list $1)

# Delete all mailboxes (exclude sub-boxes)
sep='/'
IFS=$'\n'; mbox_array=($list); unset IFS;
for mbox in "${mbox_array[@]}"; do
	if ! string_contains $mbox $sep; then
		delete_mailbox $1 $mbox || { echo "Delete $mbox failed" ; exit 1 ; } 
	fi
done

# Delete mail_location
if [ -z $mail_location ] || [ ! -d $mail_location ] ; then
	error_exit "user path is empty or doesn't exist: $mail_location"								
fi

rm -Rf $mail_location
echo $?
