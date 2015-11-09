#!/bin/bash
# this script needs to be sourced by shells wanting to use
# the gbbox based commands.


##############################################
# clean out all stale gbbox environment files

# get PIDs of this user's bash sessions
for name in `ps aux | grep $UID | grep bash | grep -v grep | awk '{print $2}'`
  do
  bash_pid[$name]=1
done

# get PIDs of all old environment files
gbbox_pids=""
gbbox_list=""

for name in /tmp/gbbox_${UID}_*
do
  if test $name != /tmp/gbbox_${UID}_'*'
    then
    gbbox_list="$gbbox_list $name"
  fi
done

if test x"$gbbox_list" != x
then
  for name in $gbbox_list
    do
    gbbox_pid=${name#/tmp/gbbox_${UID}_}
    if test x"${bash_pid[${gbbox_pid}]}" != x1
        then rm $name
        #else echo keeping active session $name
    fi
  done
fi
##############################################

# define the current gbbox environment file which is used to store
#   this shell's client environment
export GBBOX_ENVFILE=/tmp/gbbox_${UID}_$PPID

