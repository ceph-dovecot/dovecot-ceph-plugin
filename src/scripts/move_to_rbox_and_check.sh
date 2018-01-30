#!/bin/bash

#set -x 

#IDENTIFIER="`date '+%d.%m.%y %R.%S.%N'`.$$"
IDENTIFIER="`date '+%s.%N'`.$$"
FAILEXITCODE=2

function log()
{
  echo "$IDENTIFIER $*"
}

function fail()
{
  if [ "$1" != "" ]
  then
    echo "$IDENTIFIER $1"
  fi

  log "##### `date` User $USER failed"
  exit $FAILEXITCODE
}

# check if $1 is an absolute path, else fail with message $2
function check_absolute_path()
{
  if [ "${1:0:1}" != '/' ]
  then
    fail "$2"
  fi
}

function checkstatus()
{
  # $1 = exit status to check
  # $2 = command which returned the exit status
  if [ $1 -ne 0 ]
  then
    fail "command \"$2\" failed with exit status $1"
  fi
}

function execute_cmd()
{
  log "execute command \"$*\""
  "$@" > "$COMPRESS_TMP_CMDLOG" 2>&1
  local exitcode=$?
  sed -n -e "s/^/$IDENTIFIER /" "$COMPRESS_TMP_CMDLOG"
  checkstatus $exitcode "$*"
}

# like execute_cmd_with_redirect() but stderr is redirected to /dev/null
function execute_cmd_with_redirect_ignore_stderr()
{
  local redirect_stdout="$1"
  shift
  echo "$IDENTIFIER execute command \"$* > $redirect_stdout 2> /dev/null\""
  "$@" > "$redirect_stdout" 2> /dev/null
  checkstatus $? "$* > $redirect_stdout 2> /dev/null"
}

USER=$1
TMP_DIR=$2
TMP_FILE_MDBOX="$TMP_DIR/tmp_file_mdbox.txt"
TMP_FILE_RBOX="$TMP_DIR/tmp_file_rbox.txt"
COMPRESS_TMP_CMDLOG="$TMP_DIR/command.log"
DOVEADM=/home/peter/dovecot_master.2.2/bin/doveadm
MDBOX_CONF=/home/peter/dovecot_master.2.2/etc/dovecot/mdbox_dovecot.conf
RBOX_CONF=/home/peter/dovecot_master.2.2/etc/dovecot/rbox_backup_dovecot.conf
MDBOX=$($DOVEADM -c "$MDBOX_CONF" user -f mail "$USER")
RBOX=$($DOVEADM user -f mail "$USER")
DEBUG=""

# omit "date.saved" because this changes with the backup (backup is a save to a new location)
# and omit "flags" because the order of keywords is random and can be different after a backup
readonly fields_to_verify="guid date.received date.sent pop3.uidl seq size.virtual uid user mailbox-guid mailbox hdr.Subject"

function list_all_mails()
{
	local tmp_file="$1"
	local conf_file="$2"
	local fields="$3"
	log "Get list of mails for user $USER --> $tmp_file"
	if [[ -n "$conf_file" ]]; then
		execute_cmd_with_redirect_ignore_stderr "$tmp_file" "$DOVEADM" -c "$conf_file" fetch -u "$USER" "$fields" ALL
	else
		log "No conf file"
		execute_cmd_with_redirect_ignore_stderr "$tmp_file" "$DOVEADM" fetch -u "$USER" "$fields" ALL
	fi	
	log "Done"
}

# Manual check of the flags. 
function check_flags()
{
	local tmp_file_mdbox="$1"
	local tmp_file_rbox="$2"
	local conf_file_mdbox="$3"
	local conf_file_rbox="$4"

	list_all_mails "$tmp_file_mdbox" "$conf_file_mdbox" "flags"
	list_all_mails "$tmp_file_rbox" "$conf_file_rbox" "flags"

	cmp -s "$tmp_file_mdbox" "$tmp_file_rbox"
	if [[ "$?" -eq 0 ]]; then
		log "The checked flags of the user $USER are identical"
		exit 0
	fi
		
	local ARRAY_MDBOX=()
	#while IFS='' read -r line || [[ -n "$line" ]]; do
	while read -r line || [[ -n "$line" ]]; do
		if [[ "$line" =~ ^flags.* ]]; then
			#echo "Text read from file: -${line:7}-"
			ARRAY_MDBOX+=("${line:7}")
		fi
	done < "$tmp_file_mdbox"
	
	local ARRAY_RBOX=()
	#while IFS='' read -r line || [[ -n "$line" ]]; do
	while read -r line || [[ -n "$line" ]]; do
		if [[ "$line" =~ ^flags.* ]]; then
			#echo "Text read from file: -${line:7}-"
			ARRAY_RBOX+=("${line:7}")
		fi
	done < "$tmp_file_rbox"
	
	# get length of an array
	local ARRAY_MDBOX_LEN=${#ARRAY_MDBOX[@]}
	local ARRAY_RBOX_LEN=${#ARRAY_RBOX[@]}
	if [[ ARRAY_MDBOX_LEN -ne ARRAY_RBOX_LEN ]]; then
		exit 1
	fi	
	
	# use for loop read all 'flags' lines
	for (( i=0; i<${ARRAY_MDBOX_LEN}; i++ )); do
		if [[ "${ARRAY_MDBOX[$i]}" == "${ARRAY_RBOX[$i]}" ]]; then
			#echo "mdbox: ${ARRAY_MDBOX[$i]}"
			#echo "rbox: ${ARRAY_RBOX[$i]}"
			continue
		fi		
		local ARRAY_MDBOX_LINE=(${ARRAY_MDBOX[$i]})
		local ARRAY_RBOX_LINE=(${ARRAY_RBOX[$i]})
	
		local ARRAY_MDBOX_LINE_LEN=${#ARRAY_MDBOX_LINE[@]}
		local ARRAY_RBOX_LINE_LEN=${#ARRAY_RBOX_LINE[@]}
		if [[ ARRAY_MDBOX_LINE_LEN -ne ARRAY_RBOX_LINE_LEN ]]; then
			exit 1
		fi	
	
		if [[ ARRAY_MDBOX_LINE_LEN -gt 0 ]]; then
			local found=0
			for (( j=0; j<${ARRAY_MDBOX_LINE_LEN}; j++ )); do
				local mflag=(${ARRAY_MDBOX_LINE[$j]})
				for (( k=0; k<${ARRAY_RBOX_LINE_LEN}; k++ )); do
					local rflag=(${ARRAY_RBOX_LINE[$k]})
					if [[ "$rflag" == "$mflag" ]]; then
						((found++))
						break		
					fi	
				done									
			done
		
			if [[ $found -ne $ARRAY_MDBOX_LINE_LEN ]]; then
				fail "The checked flags of the user $USER are not identical"
				exit 1
			fi
		fi
	done
	
	log "The checked flags of the user $USER are identical"
}

log "$MDBOX"
log "$RBOX"

execute_cmd "$DOVEADM" -c "$MDBOX_CONF" purge -u "$USER"

#MAIL_LIST_MDBOX=$($DOVEADM -c "$MDBOX_CONF" fetch -u "$USER" "$FIELDS" ALL)
list_all_mails "$TMP_FILE_MDBOX" "$MDBOX_CONF" "$fields_to_verify"

execute_cmd "$DOVEADM" -c "$MDBOX_CONF" backup -f -u "$USER" "$RBOX"

#MAIL_LIST_RBOX=$($DOVEADM -c "$RBOX_CONF" fetch -u "$USER" "$FIELDS" ALL)
list_all_mails "$TMP_FILE_RBOX" "" "$fields_to_verify"

# $DOVEADM $DEBUG -c "$MDBOX_CONF" quota get -u "$USER"
# $DOVEADM $DEBUG quota get -u "$USER"

# optional die Listen vergleichen
#echo "$MAIL_LIST_MDBOX" > "$TMP_FILE_MDBOX"
#echo "$MAIL_LIST_RBOX" > "$TMP_FILE_RBOX"
cmp -s "$TMP_FILE_MDBOX" "$TMP_FILE_RBOX"
if [[ "$?" -ne 0 ]]; then
	fail "The checked attributes of the user $USER are not identical"
	exit 1
fi

TMP_FILE_MDBOX_FLAGS="$TMP_DIR/tmp_file_mdbox_flags.txt"
TMP_FILE_RBOX_FLAGS="$TMP_DIR/tmp_file_rbox_flags.txt"

check_absolute_path "$TMP_FILE_MDBOX_FLAGS"

check_flags "$TMP_FILE_MDBOX_FLAGS" "$TMP_FILE_RBOX_FLAGS" "$MDBOX_CONF"
exit $?
