#!/bin/bash

set -uo pipefail

PREFIX=/opt/oai-nr-ue
CONFIGFILE=$PREFIX/etc/nr-ue.conf

echo "=================================="
echo "/proc/sys/kernel/core_pattern=$(cat /proc/sys/kernel/core_pattern)"

if [ ! -f $CONFIGFILE ]; then
  echo "No configuration file $CONFIGFILE found: attempting to find YAML config"
  YAML_CONFIGFILE=$PREFIX/etc/nr-ue.yaml
  if [ ! -f $YAML_CONFIGFILE ]; then
    echo "No configuration file $YAML_CONFIGFILE found. Please mount either at $CONFIGFILE or $YAML_CONFIGFILE"
    exit 255
  fi
  CONFIGFILE=$YAML_CONFIGFILE
fi

echo "=================================="
echo "== Configuration file:"
cat $CONFIGFILE

# Load the USRP binaries
echo "=================================="
echo "== Load USRP binaries"
if [[ -v USE_B2XX ]]; then
    $PREFIX/bin/uhd_images_downloader.py -t b2xx
elif [[ -v USE_X3XX ]]; then
    $PREFIX/bin/uhd_images_downloader.py -t x3xx
elif [[ -v USE_N3XX ]]; then
    $PREFIX/bin/uhd_images_downloader.py -t n3xx
fi

# in case we have conf file, append
new_args=()
while [[ $# -gt 0 ]]; do
  new_args+=("$1")
  shift
done

new_args+=("-O")
new_args+=("$CONFIGFILE")

# enable printing of stack traces on assert
export OAI_GDBSTACKS=1

echo "=================================="
echo "== Starting NR UE soft modem"
if [[ -v USE_ADDITIONAL_OPTIONS ]]; then
    echo "Additional option(s): ${USE_ADDITIONAL_OPTIONS}"
    for word in ${USE_ADDITIONAL_OPTIONS}; do
        new_args+=("$word")
    done
    echo "${new_args[@]}"
    exec "${new_args[@]}"
else
    echo "${new_args[@]}"
    exec "${new_args[@]}"
fi
