# socket_test

A simple implementation of xstore through socket.

## build dependencies

First, prepare the submodules

[README](../README.md)

Replacing `xtore/build-config.toml` with `xtore/socket_test/build-config.toml` 

Then build socket_test, check the following document:

[BUILD](../docs/build.md)

## examples

Download OpenStreetMap (OSM) on AWS from https://aws.amazon.com/public-datasets/

To start server:

`./socket_server --nkeys=100000 --nmodels=1000 --alloc_mem_m=128`

To start client:

`./socket_client --nkeys=1000`

