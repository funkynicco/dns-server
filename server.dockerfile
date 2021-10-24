FROM mcr.microsoft.com/windows/nanoserver:1903

WORKDIR /app
COPY build/vs2019/bin/Release/x64/dns-server.exe .
COPY run/config.cfg .

EXPOSE 53

CMD ["dns-server"]