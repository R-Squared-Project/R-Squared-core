#!/bin/bash
RSQUARED="/usr/local/bin/witness_node"

# For blockchain download
VERSION=`cat /etc/rsquared/version`

## Supported Environmental Variables
#
#   * $RSQUARED_SEED_NODES
#   * $RSQUARED_RPC_ENDPOINT
#   * $RSQUARED_PLUGINS
#   * $RSQUARED_REPLAY
#   * $RSQUARED_RESYNC
#   * $RSQUARED_P2P_ENDPOINT
#   * $RSQUARED_WITNESS_ID
#   * $RSQUARED_PRIVATE_KEY
#   * $RSQUARED_SEED
#   * $RSQUARED_TRACK_ACCOUNTS
#   * $RSQUARED_PARTIAL_OPERATIONS
#   * $RSQUARED_MAX_OPS_PER_ACCOUNT
#   * $RSQUARED_ES_NODE_URL
#   * $RSQUARED_ES_START_AFTER_BLOCK
#   * $RSQUARED_TRUSTED_NODE
#

ARGS=""
# Translate environmental variables
if [[ ! -z "$RSQUARED_SEED_NODES" ]]; then
    for NODE in $RSQUARED_SEED_NODES ; do
        ARGS+=" --seed-node=$NODE"
    done
fi
if [[ ! -z "$RSQUARED_RPC_ENDPOINT" ]]; then
    ARGS+=" --rpc-endpoint=${RSQUARED_RPC_ENDPOINT}"
fi

if [[ ! -z "$RSQUARED_REPLAY" ]]; then
    ARGS+=" --replay-blockchain"
fi

if [[ ! -z "$RSQUARED_RESYNC" ]]; then
    ARGS+=" --resync-blockchain"
fi

if [[ ! -z "$RSQUARED_P2P_ENDPOINT" ]]; then
    ARGS+=" --p2p-endpoint=${RSQUARED_P2P_ENDPOINT}"
fi

if [[ ! -z "$RSQUARED_WITNESS_ID" ]]; then
    ARGS+=" --witness-id=$RSQUARED_WITNESS_ID"
fi

if [[ ! -z "$RSQUARED_PRIVATE_KEY" ]]; then
    ARGS+=" --private-key=$RSQUARED_PRIVATE_KEY"
fi

if [[ ! -z "$RSQUARED_SEED" ]]; then
    ARGS+=" --user-provided-seed=$RSQUARED_SEED"
fi

if [[ ! -z "$RSQUARED_TRACK_ACCOUNTS" ]]; then
    for ACCOUNT in $RSQUARED_TRACK_ACCOUNTS ; do
        ARGS+=" --track-account=$ACCOUNT"
    done
fi

if [[ ! -z "$RSQUARED_PARTIAL_OPERATIONS" ]]; then
    ARGS+=" --partial-operations=${RSQUARED_PARTIAL_OPERATIONS}"
fi

if [[ ! -z "$RSQUARED_MAX_OPS_PER_ACCOUNT" ]]; then
    ARGS+=" --max-ops-per-account=${RSQUARED_MAX_OPS_PER_ACCOUNT}"
fi

if [[ ! -z "$RSQUARED_ES_NODE_URL" ]]; then
    ARGS+=" --elasticsearch-node-url=${RSQUARED_ES_NODE_URL}"
fi

if [[ ! -z "$RSQUARED_ES_START_AFTER_BLOCK" ]]; then
    ARGS+=" --elasticsearch-start-es-after-block=${RSQUARED_ES_START_AFTER_BLOCK}"
fi

if [[ ! -z "$RSQUARED_TRUSTED_NODE" ]]; then
    ARGS+=" --trusted-node=${RSQUARED_TRUSTED_NODE}"
fi

## Link the rsquared config file into home
## This link has been created in Dockerfile, already
ln -f -s /etc/rsquared/config.ini /var/lib/rsquared
ln -f -s /etc/rsquared/logging.ini /var/lib/rsquared

# Plugins need to be provided in a space-separated list, which
# makes it necessary to write it like this
if [[ ! -z "$RSQUARED_PLUGINS" ]]; then
   exec "$RSQUARED" --data-dir "${HOME}" ${ARGS} ${RSQUARED_ARGS} --plugins "${RSQUARED_PLUGINS}"
else
   exec "$RSQUARED" --data-dir "${HOME}" ${ARGS} ${RSQUARED_ARGS}
fi
