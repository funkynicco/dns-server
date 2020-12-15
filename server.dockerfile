FROM mcr.microsoft.com/windows/nanoserver:1903
#FROM mcr.microsoft.com/windows/servercore:1903
#FROM mcr.microsoft.com/windows:1903
WORKDIR /app

COPY ./x64/RelNano/DnsServerTest.exe .
COPY ./x64/RelNano/tmptest1.exe .
COPY ./DnsServerTest/config.cfg .

EXPOSE 53
#CMD ["DnsServerTest.exe"]
CMD ["cmd"]