#!/bin/bash

# This script includes several doveadm functions.

set -o errexit
#set -o nounset

DOVEADM="/opt/app/dovecot/bin/doveadm"
#DOVEADM="/home/peter/dovecot_master.2.2/bin/doveadm"
#export $DOVEADM

# Tests if required environment variable exist
#
#test_environment() {
#	if [ -n "$DOVECOT_HOME" ] && [ -d "$DOVECOT_HOME" ]; then
#		echo "DOVECOT_HOME is set: $DOVECOT_HOME"
#	else 
#		error_exit "DOVECOT_HOME not set"
#	fi		
#}

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
	local dest_path="$1"
	local user="$2"
	local mail_format="$3"
	
	if [ ! -x "$DOVEADM" ]; then
		error_exit "doveadm doesn't exist: $DOVEADM"								
	fi
	
	if [ "$mail_format" = "rbox" ]; then
		"$DOVEADM" sync -u "$user" "$mail_format":"$dest_path"/"$user":LAYOUT=fs
	else
		"$DOVEADM" sync -u "$user" "$mail_format":"$dest_path"/"$user"
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
	local ml_array=()
	local path
	
	if [ ! -x "$DOVEADM" ]; then
		error_exit "doveadm doesn't exist: $DOVEADM"								
	fi
	
	mail=$("$DOVEADM" user -f mail "$user")
	IFS=":" read -r -a ml_array <<< "$mail"
	#IFS=$':'; ml_array=("$mail"); unset IFS;
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
	
	if [ ! -x "$DOVEADM" ]; then
		error_exit "doveadm doesn't exist: $DOVEADM"								
	fi
	
	list=$("$DOVEADM" mailbox list -u "$user")
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
	
	if [ ! -x "$DOVEADM" ]; then
		error_exit "doveadm doesn't exist: $DOVEADM"								
	fi
	
	"$DOVEADM" mailbox delete -rsZ -u "$user" "$mailbox" &> /dev/null
}
