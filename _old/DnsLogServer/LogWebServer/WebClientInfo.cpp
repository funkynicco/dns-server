#include "StdAfx.h"
#include "LogWebServer.h"

void WEB_CLIENT_INFO::SendCodeResult(int code, const char* code_message, const unordered_map<string, string>& extra_parameters, const char* html)
{
	int html_len = (int)strlen(html);

	char data[32768];
	char* ptr = data;

	ptr += sprintf(
		ptr,
		"HTTP/1.1 %d %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\n",
		code,
		code_message,
		html_len);

	for (unordered_map<string, string>::const_iterator it = extra_parameters.begin(); it != extra_parameters.end(); ++it)
	{
		ptr += sprintf(
			ptr,
			"%s: %s\r\n",
			it->first.c_str(),
			it->second.c_str());
	}

	*ptr++ = '\r';
	*ptr++ = '\n';

	memcpy(ptr, html, html_len);
	ptr += html_len;

	lpClient->Send(data, int(ptr - data));
}

void WEB_CLIENT_INFO::SendNotFoundPage()
{
	char html[2048];
	sprintf(html, "<h1>Not Found</h1><p>404 - The requested resource could not be found.</p>");

	unordered_map<string, string> extra_parameters;
	SendCodeResult(404, "Not Found", extra_parameters, html);
}

void WEB_CLIENT_INFO::SendHeader(int code, const char* code_message, const unordered_map<string, string>& parameters)
{
	char header[8192];
	char* pheader = header;

	pheader += sprintf(
		pheader,
		"HTTP/1.1 %d %s\r\n",
		code,
		code_message);

	for (unordered_map<string, string>::const_iterator it = parameters.cbegin(); it != parameters.cend(); ++it)
	{
		pheader += sprintf(
			pheader,
			"%s: %s\r\n",
			it->first.c_str(),
			it->second.c_str());
	}

	*pheader++ = '\r';
	*pheader++ = '\n';
	*pheader = 0;

	lpClient->Send(header, int(pheader - header));
}