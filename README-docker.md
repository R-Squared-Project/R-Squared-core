# Docker Container

This repository comes with built-in Dockerfile to support docker
containers. This README serves as documentation.

## Dockerfile Specifications

The `Dockerfile` performs the following steps:

1. Obtain base image (phusion/baseimage:18.04-1.0.0)
2. Install required dependencies using `apt`
3. Add rsquared-core source code into container
4. Update git submodules
5. Perform `cmake` with build type `Release`
6. Run `make` and `make_install` (this will install binaries into `/usr/local/bin`
7. Purge source code off the container
8. Add a local rsquared user and set `$HOME` to `/var/lib/rsquared`
9. Make `/var/lib/rsquared` and `/etc/rsquared` a docker *volume*
10. Expose ports `8090` and `2771`
11. Add default config from `docker/default_config.ini` and entry point script
12. Run entry point script by default

The entry point simplifies the use of parameters for the `witness_node`
(which is run by default when spinning up the container).

You can launch a build process with a command
```sh
$ docker build $RSQUARED_CORE_DIR -t local/rsquared-core:latest
```

### Supported Environmental Variables

* `$RSQUAREDD_SEED_NODES`
* `$RSQUAREDD_RPC_ENDPOINT`
* `$RSQUAREDD_PLUGINS`
* `$RSQUAREDD_REPLAY`
* `$RSQUAREDD_RESYNC`
* `$RSQUAREDD_P2P_ENDPOINT`
* `$RSQUAREDD_WITNESS_ID`
* `$RSQUAREDD_PRIVATE_KEY`
* `$RSQUAREDD_TRACK_ACCOUNTS`
* `$RSQUAREDD_PARTIAL_OPERATIONS`
* `$RSQUAREDD_MAX_OPS_PER_ACCOUNT`
* `$RSQUAREDD_ES_NODE_URL`
* `$RSQUAREDD_TRUSTED_NODE`

### Default config

The default configuration is:

    p2p-endpoint = 0.0.0.0:2771
    rpc-endpoint = 0.0.0.0:8090
    enable-stale-production = false
    max-ops-per-account = 1000
    partial-operations = true

# Docker Compose

With docker compose, multiple nodes can be managed with a single
`docker-compose.yaml` file:

    version: '3'
    services:
     main:
      # Image to run
      image: local/rsquared-core:latest
      # 
      volumes:
       - ./docker/conf/:/etc/rsquared/
      # Optional parameters
      environment:
       - RSQUAREDD_ARGS=--help

or

    version: '3'
    services:
     fullnode:
      # Image to run
      image: local/rsquared-core:latest
      environment:
      # Optional parameters
       - RSQUAREDD_ARGS=--help
      ports:
       - "0.0.0.0:8090:8090"
      volumes:
      - "rsquared-fullnode:/var/lib/rsquared"


# GitHub Container registry (GHCR)

This container is properly registered with the GitHub Container registry:

```sh
$ docker pull ghcr.io/r-squared-project/r-squared-core:latest
```

# Docker Compose

One can use docker compose to setup a trusted full node together with a
delayed node like this:

```
version: '3'
services:

 fullnode:
  image: ghcr.io/r-squared-project/r-squared-core:latest
  ports:
   - "0.0.0.0:8090:8090"
  volumes:
  - "rsquared-fullnode:/var/lib/rsquared"

 delayed_node:
  image: ghcr.io/r-squared-project/r-squared-core:latest
  environment:
   - 'RSQUAREDD_PLUGINS=delayed_node witness'
   - 'RSQUAREDD_TRUSTED_NODE=ws://fullnode:8090'
  ports:
   - "0.0.0.0:8091:8090"
  volumes:
  - "rsquared-delayed_node:/var/lib/rsquared"
  links: 
  - fullnode

volumes:
 rsquared-fullnode:
```
