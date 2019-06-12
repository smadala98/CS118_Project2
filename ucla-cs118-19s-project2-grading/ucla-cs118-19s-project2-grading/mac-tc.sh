#!/bin/bash
# Script to tune traffic on macOS
# Author: Zengwen Yuan <zyuan@cs.ucla.edu>
# Reference: https://serverfault.com/questions/725030
# Usage:  ./mac-tc.sh [server_port_number] [loss rate for client's outgoing traffic] [loss rate for server's outgoing traffic]

throttle_start() {
  # Reset dummynet to default config
  sudo dnctl -f flush

  # get parameters from command line arguments
  local port_num="$1"
  local loss_rate_in="$2"
  local loss_rate_out="$3"

  # create filter rules for testing
  sudo dnctl pipe 1 config plr "${loss_rate_in}"
  sudo dnctl pipe 2 config plr "${loss_rate_out}"

  # configure the new anchor
  (cat /etc/pf.conf && \
    echo 'dummynet-anchor "throttle"' && \
    echo 'anchor "throttle"') | sudo pfctl -f -

  # add rules to the "throttle" set to send any traffic to endpoint through new rule
  cat << EOF | sudo pfctl -a throttle -f -
dummynet in quick proto udp from any to any port = ${port_num} pipe 1
dummynet out quick proto udp from any port = ${port_num} to any pipe 2
EOF

  # activate PF
  sudo pfctl -q -e

  # check that dnctl is properly configured: sudo dnctl list
  sudo dnctl list
}

throttle_start "$@"
