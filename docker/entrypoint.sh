#!/bin/bash
RSQUAREDD="/usr/local/bin/witness_node"

# For blockchain download
VERSION=`cat /etc/rsquared/version`

## Supported Environmental Variables
#
#   * $RSQUAREDD_SEED_NODES
#   * $RSQUAREDD_RPC_ENDPOINT
#   * $RSQUAREDD_PLUGINS
#   * $RSQUAREDD_REPLAY
#   * $RSQUAREDD_RESYNC
#   * $RSQUAREDD_P2P_ENDPOINT
#   * $RSQUAREDD_WITNESS_ID
#   * $RSQUAREDD_PRIVATE_KEY
#   * $RSQUAREDD_SEED
#   * $RSQUAREDD_TRACK_ACCOUNTS
#   * $RSQUAREDD_PARTIAL_OPERATIONS
#   * $RSQUAREDD_MAX_OPS_PER_ACCOUNT
#   * $RSQUAREDD_ES_NODE_URL
#   * $RSQUAREDD_ES_START_AFTER_BLOCK
#   * $RSQUAREDD_TRUSTED_NODE
#

ARGS=""
# Translate environmental variables
if [[ ! -z "$RSQUAREDD_SEED_NODES" ]]; then
    for NODE in $RSQUAREDD_SEED_NODES ; do
        ARGS+=" --seed-node=$NODE"
    done
fi
if [[ ! -z "$RSQUAREDD_RPC_ENDPOINT" ]]; then
    ARGS+=" --rpc-endpoint=${RSQUAREDD_RPC_ENDPOINT}"
fi

if [[ ! -z "$RSQUAREDD_REPLAY" ]]; then
    ARGS+=" --replay-blockchain"
fi

if [[ ! -z "$RSQUAREDD_RESYNC" ]]; then
    ARGS+=" --resync-blockchain"
fi

if [[ ! -z "$RSQUAREDD_P2P_ENDPOINT" ]]; then
    ARGS+=" --p2p-endpoint=${RSQUAREDD_P2P_ENDPOINT}"
fi

if [[ ! -z "$RSQUAREDD_WITNESS_ID" ]]; then
    ARGS+=" --witness-id=$RSQUAREDD_WITNESS_ID"
fi

if [[ ! -z "$RSQUAREDD_PRIVATE_KEY" ]]; then
    ARGS+=" --private-key=$RSQUAREDD_PRIVATE_KEY"
fi

if [[ ! -z "$RSQUAREDD_SEED" ]]; then
    ARGS+=" --user-provided-seed=$RSQUAREDD_SEED"
fi

if [[ ! -z "$RSQUAREDD_TRACK_ACCOUNTS" ]]; then
    for ACCOUNT in $RSQUAREDD_TRACK_ACCOUNTS ; do
        ARGS+=" --track-account=$ACCOUNT"
    done
fi

if [[ ! -z "$RSQUAREDD_PARTIAL_OPERATIONS" ]]; then
    ARGS+=" --partial-operations=${RSQUAREDD_PARTIAL_OPERATIONS}"
fi

if [[ ! -z "$RSQUAREDD_MAX_OPS_PER_ACCOUNT" ]]; then
    ARGS+=" --max-ops-per-account=${RSQUAREDD_MAX_OPS_PER_ACCOUNT}"
fi

if [[ ! -z "$RSQUAREDD_ES_NODE_URL" ]]; then
    ARGS+=" --elasticsearch-node-url=${RSQUAREDD_ES_NODE_URL}"
fi

if [[ ! -z "$RSQUAREDD_ES_START_AFTER_BLOCK" ]]; then
    ARGS+=" --elasticsearch-start-es-after-block=${RSQUAREDD_ES_START_AFTER_BLOCK}"
fi

if [[ ! -z "$RSQUAREDD_TRUSTED_NODE" ]]; then
    ARGS+=" --trusted-node=${RSQUAREDD_TRUSTED_NODE}"
fi

## Link the rsquared config file into home
## This link has been created in Dockerfile, already
ln -f -s /etc/rsquared/config.ini /var/lib/rsquared
ln -f -s /etc/rsquared/logging.ini /var/lib/rsquared

# Plugins need to be provided in a space-separated list, which
# makes it necessary to write it like this
if [[ ! -z "$RSQUAREDD_PLUGINS" ]]; then
   exec "$RSQUAREDD" --data-dir "${HOME}" ${ARGS} ${RSQUAREDD_ARGS} --plugins "${RSQUAREDD_PLUGINS}"
else
   exec "$RSQUAREDD" --data-dir "${HOME}" ${ARGS} ${RSQUAREDD_ARGS}
fi
