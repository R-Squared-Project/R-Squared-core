R-Squared Core
==============

* [Getting Started](#getting-started)
* [Using the API](#using-the-api)

R-Squared Core is the R-Squared blockchain implementation and command-line interface.

|Branch|Build Status|
|---|---|
|`master`|[![](https://github.com/R-Squared-Project/R-Squared-core/workflows/Ubuntu%20Release/badge.svg?branch=master)](https://github.com/R-Squared-Project/R-Squared-core/actions?query=workflow%3A"Ubuntu+Release"+branch%3Amaster) [![](https://github.com/R-Squared-Project/R-Squared-core/workflows/Ubuntu%20Debug/badge.svg?branch=master)](https://github.com/R-Squared-Project/R-Squared-core/actions?query=workflow%3A"Ubuntu+Debug"+branch%3Amaster)|
|`development`|[![](https://github.com/R-Squared-Project/R-Squared-core/workflows/Ubuntu%20Release/badge.svg?branch=development)](https://github.com/R-Squared-Project/R-Squared-core/actions?query=workflow%3A"Ubuntu+Release"+branch%3Adevelopment) [![](https://github.com/R-Squared-Project/R-Squared-core/workflows/Ubuntu%20Debug/badge.svg?branch=development)](https://github.com/R-Squared-Project/R-Squared-core/actions?query=workflow%3A"Ubuntu+Debug"+branch%3Adevelopment)|

Quick start using Docker
-----------------------

A Dockerfile can be used to build and run a fully functional blockchain node. [Refer to the instructions](README-docker.md).

Getting Started
---------------
Build instructions and additional documentation are available in the
[Wiki](https://github.com/R-Squared-Project/R-Squared-core/wiki).

We recommend building on Ubuntu 18.04 LTS (64-bit)

**Build Dependencies:**

    sudo apt-get update
    sudo apt-get install autoconf cmake make automake libtool git libboost-all-dev libssl-dev g++ libcurl4-openssl-dev doxygen

**Build Script:**

    git clone https://github.com/R-Squared-Project/R-Squared-core.git
    cd R-Squared-core
    git checkout master # may substitute "master" with current release tag
    git submodule update --init --recursive
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make

**Upgrade Script:** (prepend to the Build Script above if you built a prior release):

    git remote set-url origin https://github.com/R-Squared-Project/R-Squared-core.git
    git checkout master
    git remote set-head origin --auto
    git pull
    git submodule update --init --recursive # this command may fail
    git submodule sync --recursive
    git submodule update --init --recursive

**NOTE:** Versions of [Boost](http://www.boost.org/) since 1.58 are supported. Newer versions may work, but
have not been tested well. If your system came pre-installed with a version of Boost that you do not wish to use, you may
manually build your preferred version and use it with R-Squared by specifying it on the CMake command line.

  Example: `cmake -DBOOST_ROOT=/path/to/boost ..`

**NOTE:** R-Squared requires a 64-bit operating system to build, and will not build on a 32-bit OS.

**After Building**, the `witness_node` can be launched with:

    ./programs/witness_node/witness_node

The node will automatically create a data directory including a config file. You can exit the node using Ctrl+C and setup the command-line wallet by editing
`witness_node_data_dir/config.ini` as follows:

    rpc-endpoint = 127.0.0.1:8090

After starting the witness node again, in a separate terminal you can run:

    ./programs/cli_wallet/cli_wallet

Set your inital password:

    >>> set_password <PASSWORD>
    >>> unlock <PASSWORD>

**IMPORTANT:** The cli_wallet or API interfaces to the witness node wouldn't be fully functional unless the witness node is fully synchronized with the blockchain. The cli_wallet command `info` will show result `head_block_age` which will tell you how far you are from the live current block of the blockchain.


To check your current block:

    >>> info

To import your initial balance:

    >>> import_balance <ACCOUNT NAME> [<WIF_KEY>] true

If you send private keys over this connection, `rpc-endpoint` should be bound to localhost for security.

Use `help` to see all available wallet commands. Source definition and listing of all commands is available
[here](https://github.com/R-Squared-Project/R-Squared-core/blob/master/libraries/wallet/include/graphene/wallet/wallet.hpp).

Using the API
-------------

We provide several different API's.  Each API has its own ID.
When running `witness_node`, initially two API's are available:
API 0 provides read-only access to the database, while API 1 is
used to login and gain access to additional, restricted API's.

Here is an example using `wscat` package from `npm` for websockets:

    $ npm install -g wscat
    $ wscat -c ws://127.0.0.1:8090
    > {"id":1, "method":"call", "params":[0,"get_accounts",[["1.2.0"]]]}
    < {"id":1,"result":[{"id":"1.2.0","annotations":[],"membership_expiration_date":"1969-12-31T23:59:59","registrar":"1.2.0","referrer":"1.2.0","lifetime_referrer":"1.2.0","network_fee_percentage":2000,"lifetime_referrer_fee_percentage":8000,"referrer_rewards_percentage":0,"name":"committee-account","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[],"address_auths":[]},"active":{"weight_threshold":6,"account_auths":[["1.2.5",1],["1.2.6",1],["1.2.7",1],["1.2.8",1],["1.2.9",1],["1.2.10",1],["1.2.11",1],["1.2.12",1],["1.2.13",1],["1.2.14",1]],"key_auths":[],"address_auths":[]},"options":{"memo_key":"GPH1111111111111111111111111111111114T1Anm","voting_account":"1.2.0","num_witness":0,"num_committee":0,"votes":[],"extensions":[]},"statistics":"2.7.0","whitelisting_accounts":[],"blacklisting_accounts":[]}]}

We can do the same thing using an HTTP client such as `curl` for API's which do not require login or other session state:

    $ curl --data '{"jsonrpc": "2.0", "method": "call", "params": [0, "get_accounts", [["1.2.0"]]], "id": 1}' http://127.0.0.1:8090/rpc
    {"id":1,"result":[{"id":"1.2.0","annotations":[],"membership_expiration_date":"1969-12-31T23:59:59","registrar":"1.2.0","referrer":"1.2.0","lifetime_referrer":"1.2.0","network_fee_percentage":2000,"lifetime_referrer_fee_percentage":8000,"referrer_rewards_percentage":0,"name":"committee-account","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[],"address_auths":[]},"active":{"weight_threshold":6,"account_auths":[["1.2.5",1],["1.2.6",1],["1.2.7",1],["1.2.8",1],["1.2.9",1],["1.2.10",1],["1.2.11",1],["1.2.12",1],["1.2.13",1],["1.2.14",1]],"key_auths":[],"address_auths":[]},"options":{"memo_key":"GPH1111111111111111111111111111111114T1Anm","voting_account":"1.2.0","num_witness":0,"num_committee":0,"votes":[],"extensions":[]},"statistics":"2.7.0","whitelisting_accounts":[],"blacklisting_accounts":[]}]}

API 0 is accessible using regular JSON-RPC:

    $ curl --data '{"jsonrpc": "2.0", "method": "get_accounts", "params": [["1.2.0"]], "id": 1}' http://127.0.0.1:8090/rpc

Contributing
------------

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

Please make sure to update tests as appropriate.

License
-------

[MIT](https://choosealicense.com/licenses/mit/)
