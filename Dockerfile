FROM ubuntu:16.04

RUN apt-get update && apt-get -qy install make g++

ADD Makefile *h *cpp /tmp/pbft-src/
