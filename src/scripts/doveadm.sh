#!/bin/bash

# This script includes several doveadm functions.

set -o errexit
#set -o nounset

# Tests if required environment variable exist
#
test_environment() {
	if [ -n "$DOVECOT_HOME" ] && [ -d "$DOVECOT_HOME" ]; then
		echo "DOVECOT_HOME is set: $DOVECOT_HOME"
	else 
		error_exit "DOVECOT_HOME not set"
	fi		
}

# Calls doveadm sync and returns its exit value
# doveadm will exit with one of the following values:
# 0  Selected command was executed successful.
# >0 Command failed in some way
#
# Parameter $1 destination path
# Parameter $2 user id
# Parameter $3 mail format
#
doveadm_sync() {
	local bin_path="$DOVECOT_HOME/bin"
	local dest_path="$1"
	local user="$2"
	local mail_format="$3"
	
	if [ -z "$bin_path" ] || [ ! -d "$bin_path" ]; then
		error_exit "program path is empty or doesn't exist: $bin_path"								
	fi
	
	if [ "$mail_format" = "rbox" ]; then
		"$bin_path"/doveadm sync -u "$user" "$mail_format":"$dest_path"/"$user":LAYOUT=fs
	else
		"$bin_path"/doveadm sync -u "$user" "$mail_format":"$dest_path"/"$user"
	fi
	echo $?	
}

# Calls doveadm user and returns the user's mail location
#
# Parameter $1 user id
#
get_user_mail_location() {
	local bin_path="$DOVECOT_HOME/bin"
	local user="$1"
	local mail
	local path
	
	if [ -z "$bin_path" ] || [ ! -d "$bin_path" ]; then
		error_exit "program path is empty or doesn't exist: $bin_path"								
	fi
	
	mail=$("$bin_path"/doveadm user -f mail "$user")
	IFS=$':'; ml_array=($mail); unset IFS;
	if [ ${#ml_array[*]} -lt 2 ]; then
		error_exit "mail_location in wrong format: $mail"								
	fi
	
	path="${ml_array[1]}"
	echo "$path"
}

# Returns a list of all mailboxes of the given user
#
# Parameter $1 user id
#
get_mailbox_list() {
	local bin_path="$DOVECOT_HOME/bin"
	local user="$1"
	local list
	
	if [ -z "$bin_path" ] || [ ! -d "$bin_path" ]; then
		error_exit "program path is empty or doesn't exist: $bin_path"								
	fi
	
	list=$("$bin_path"/doveadm mailbox list -u "$user")
	echo "$list"
}

# Deletes the given mailbox of the given user
#
# Parameter $1 user id
# Parameter $2 mailbox
#
delete_mailbox() {
	local bin_path="$DOVECOT_HOME/bin"
	local user="$1"
	local mailbox="$2"
	
	if [ -z "$bin_path" ] || [ ! -d "$bin_path" ]; then
		error_exit "program path is empty or doesn't exist: $bin_path"								
	fi
	
	"$bin_path"/doveadm mailbox delete -rsZ -u "$user" "$mailbox" &> /dev/null
}
