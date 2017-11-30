#!/bin/bash

# main

# Loads utils script
script_path=${0%/*}
source $script_path/utils.sh

# Reads mail_location of user
mail_location=$(get_user_mail_location $1)

# Synchronize from mdbox to rbox
result=$(doveadm_sync $2 $1 rbox)
[ $result -eq 0 ] || { echo "doveadm sync failed"; exit 1; }

# Copies files from mdbox to rbox
copy_files $mail_location $2 $1 
echo $?
