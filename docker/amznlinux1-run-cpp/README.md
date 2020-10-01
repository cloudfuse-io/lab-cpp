# Emulation of the lambda environment

These docker/compose configurations are meant to help emulate the lambda environment locally. 

## Linux version
Lambda custom runtime (used by C++) run AmazonLinux1. Use the makefile to run this infrastructure to ensure dependency tree of images is properly built.

## Latency proxy
We have implemented an nginx proxy to simulate the network latencies in the cloud. It is far from perfect, especially the bandwidth throttling, but helps running in conditions a bit closer to production one. 