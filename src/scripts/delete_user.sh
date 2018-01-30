#!/bin/bash

# Removes all mailboxes an the mail location of the given user
#
# Parameter $1 user id
# Parameter $2 result of the move process
#
user=$1
result=$2

# Load utility script
script_path=${0%/*}
source "$script_path/utils.sh"
source "$script_path/doveadm.sh"

if [ ! $# -eq 2 ]; then
	 error_exit "Wrong number of arguments. Usage: $0 <user id> <result>"
fi
	  
if [ ! "$result" == "0" ]; then
	 error_exit "Result of move process not 0"
fi

# Get all mailboxes
list=$(get_mailbox_list "$user")
		
# Delete all mailboxes (exclude sub-boxes)
sep='/'
IFS=$'\n'; mbox_array=("$list"); unset IFS;
for mbox in "${mbox_array[@]}"; do
	if ! string_contains "$mbox" "$sep"; then
		delete_mailbox "$user" "$mbox" || { echo "Delete $mbox failed" ; exit 1 ; } 
	fi
done

# Reads mail_location of user
mail_location=$(get_user_mail_location "$user")

# Delete mail_location
if [[ -z "$mail_location" ]] || [[ ! -d "$mail_location" ]]; then
	error_exit "user path is empty or doesn't exist: $mail_location"								
fi
rm -Rf "$mail_location"

echo $?
