@echo off
docker build -t dns .
docker run -it -p 53:6466/udp dns
