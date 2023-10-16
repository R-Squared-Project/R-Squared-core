#!/bin/bash
RSQUAEDD="/usr/local/bin/witness_node"

# For blockchain download
VERSION=`cat /etc/rsquared/version`

## Supported Environmental Variables
#
#   * $RSQUAEDD_SEED_NODES
#   * $RSQUAEDD_RPC_ENDPOINT
#   * $RSQUAEDD_PLUGINS
#   * $RSQUAEDD_REPLAY
#   * $RSQUAEDD_RESYNC
#   * $RSQUAEDD_P2P_ENDPOINT
#   * $RSQUAEDD_WITNESS_ID
#   * $RSQUAEDD_PRIVATE_KEY
#   * $RSQUAEDD_SEED
#   * $RSQUAEDD_TRACK_ACCOUNTS
#   * $RSQUAEDD_PARTIAL_OPERATIONS
#   * $RSQUAEDD_MAX_OPS_PER_ACCOUNT
#   * $RSQUAEDD_ES_NODE_URL
#   * $RSQUAEDD_ES_START_AFTER_BLOCK
#   * $RSQUAEDD_TRUSTED_NODE
#

ARGS=""
# Translate environmental variables
if [[ ! -z "$RSQUAEDD_SEED_NODES" ]]; then
    for NODE in $RSQUAEDD_SEED_NODES ; do
        ARGS+=" --seed-node=$NODE"
    done
fi
if [[ ! -z "$RSQUAEDD_RPC_ENDPOINT" ]]; then
    ARGS+=" --rpc-endpoint=${RSQUAEDD_RPC_ENDPOINT}"
fi

if [[ ! -z "$RSQUAEDD_REPLAY" ]]; then
    ARGS+=" --replay-blockchain"
fi

if [[ ! -z "$RSQUAEDD_RESYNC" ]]; then
    ARGS+=" --resync-blockchain"
fi

if [[ ! -z "$RSQUAEDD_P2P_ENDPOINT" ]]; then
    ARGS+=" --p2p-endpoint=${RSQUAEDD_P2P_ENDPOINT}"
fi

if [[ ! -z "$RSQUAEDD_WITNESS_ID" ]]; then
    ARGS+=" --witness-id=$RSQUAEDD_WITNESS_ID"
fi

if [[ ! -z "$RSQUAEDD_PRIVATE_KEY" ]]; then
    ARGS+=" --private-key=$RSQUAEDD_PRIVATE_KEY"
fi

if [[ ! -z "$RSQUAEDD_SEED" ]]; then
    ARGS+=" --user-provided-seed=$RSQUAEDD_SEED"
fi

if [[ ! -z "$RSQUAEDD_TRACK_ACCOUNTS" ]]; then
    for ACCOUNT in $RSQUAEDD_TRACK_ACCOUNTS ; do
        ARGS+=" --track-account=$ACCOUNT"
    done
fi

if [[ ! -z "$RSQUAEDD_PARTIAL_OPERATIONS" ]]; then
    ARGS+=" --partial-operations=${RSQUAEDD_PARTIAL_OPERATIONS}"
fi

if [[ ! -z "$RSQUAEDD_MAX_OPS_PER_ACCOUNT" ]]; then
    ARGS+=" --max-ops-per-account=${RSQUAEDD_MAX_OPS_PER_ACCOUNT}"
fi

if [[ ! -z "$RSQUAEDD_ES_NODE_URL" ]]; then
    ARGS+=" --elasticsearch-node-url=${RSQUAEDD_ES_NODE_URL}"
fi

if [[ ! -z "$RSQUAEDD_ES_START_AFTER_BLOCK" ]]; then
    ARGS+=" --elasticsearch-start-es-after-block=${RSQUAEDD_ES_START_AFTER_BLOCK}"
fi

if [[ ! -z "$RSQUAEDD_TRUSTED_NODE" ]]; then
    ARGS+=" --trusted-node=${RSQUAEDD_TRUSTED_NODE}"
fi

## Link the rsquared config file into home
## This link has been created in Dockerfile, already
ln -f -s /etc/rsquared/config.ini /var/lib/rsquared
ln -f -s /etc/rsquared/logging.ini /var/lib/rsquared

# Plugins need to be provided in a space-separated list, which
# makes it necessary to write it like this
if [[ ! -z "$RSQUAEDD_PLUGINS" ]]; then
   exec "$RSQUAEDD" --data-dir "${HOME}" ${ARGS} ${RSQUAEDD_ARGS} --plugins "${RSQUAEDD_PLUGINS}"
else
   exec "$RSQUAEDD" --data-dir "${HOME}" ${ARGS} ${RSQUAEDD_ARGS}
fi
