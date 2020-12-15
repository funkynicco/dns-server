inline const char* GetValidateResultText(ValidateResult result)
{
	switch (result)
	{
	case VALIDATE_OK:					return "OK";
	case VALIDATE_INVALID_METHOD:		return "Invalid method";
	case VALIDATE_INVALID_PATH:			return "Invalid path";
	case VALIDATE_INVALID_PROTOCOL:		return "Invalid protocol";
	case VALIDATE_INVALID_UPGRADE:		return "Invalid upgrade";
	case VALIDATE_INVALID_CONNECTION:	return "Invalid connection";
	case VALIDATE_KEY_NOT_SPECIFIED:	return "Key not specified";
	default:							return "Unspecified error";
	}
}

inline ValidateResult ValidateWebSocketHandshake(LPWEB_CLIENT_INFO lpWebClientInfo, char* key)
{
	if (strcmp(lpWebClientInfo->Http.szMethod, "GET") != 0)
		return VALIDATE_INVALID_METHOD;

	if (*lpWebClientInfo->Http.szPath == 0)
		return VALIDATE_INVALID_PATH;

	if (strcmp(lpWebClientInfo->Http.szHttpVersion, "HTTP/1.1") != 0)
		return VALIDATE_INVALID_PROTOCOL;

	unordered_map<string, string>& param = lpWebClientInfo->Http.parameters;
	unordered_map<string, string>::iterator it = param.find("upgrade");

	// Upgrade: websocket
	if (it == param.end() ||
		_strcmpi(it->second.c_str(), "websocket") != 0)
		return VALIDATE_INVALID_UPGRADE;

	// Connection: Upgrade
	if ((it = param.find("connection")) == param.end() ||
		_strcmpi(it->second.c_str(), "upgrade") != 0)
		return VALIDATE_INVALID_CONNECTION;

	// Sec-WebSocket-Key: 27/HCF7TLvuz7BEeDnttWQ==
	if ((it = param.find("sec-websocket-key")) == param.end())
		return VALIDATE_KEY_NOT_SPECIFIED;

	strcpy(key, it->second.c_str());

	return VALIDATE_OK;
}

inline void SendResponse(LPCLIENT pc, const char* format, ...)
{
	char _text[1024];
	char* text = _text;
	va_list l;
	va_start(l, format);
	int len = _vscprintf(format, l);
	if (len >= 1024)
		text = (char*)malloc(len + 1);

	vsprintf(text, format, l);
	va_end(l);

	//.............................

	unsigned char data[8192];
	memset(data, 0, 10);

	long long data_len = strlen(text);

	data[0] = 129; // unknown (0x81)
	int mask_offset = 0;

	//data[ 1 ] = 11; // length of "hello world"
	if (data_len <= 125) // WebSocketStreamSize::SmallStream
	{
		data[1] = (unsigned char)data_len;
		mask_offset = 2; // WebSocketMaskOffset::SmallOffset
	}
	else if (data_len > 125 && data_len <= 65535) // WebSocketStreamSize::MediumStream
	{
		data[1] = 126;
		data[2] = (unsigned char)((data_len >> 8) & 0xff);
		data[3] = (unsigned char)(data_len & 0xff);
		mask_offset = 4; // WebSocketMaskOffset::MediumOffset
	}
	else // WebSocketStreamSize::BigStream
	{
		data[1] = 127;
		data[2] = (unsigned char)((data_len >> 56) & 0xff);
		data[3] = (unsigned char)((data_len >> 48) & 0xff);
		data[4] = (unsigned char)((data_len >> 40) & 0xff);
		data[5] = (unsigned char)((data_len >> 32) & 0xff);
		data[6] = (unsigned char)((data_len >> 24) & 0xff);
		data[7] = (unsigned char)((data_len >> 16) & 0xff);
		data[8] = (unsigned char)((data_len >> 8) & 0xff);
		data[9] = (unsigned char)(data_len & 0xff);
		mask_offset = 10; // WebSocketMaskOffset::BigOffset
	}

	strcpy((char*)data + mask_offset, text);

	pc->Send(reinterpret_cast<LPCSTR>(data), (int)strlen(text) + mask_offset);

	if (text != _text)
		free(text);
}

inline BOOL ReadHeader(LPCLIENT pc, unsigned char* data, int len, WebSocketStreamHeader& header)
{
	if (len >= 6)
	{
		ZeroMemory(&header, sizeof(header));

		header.fin = (data[0] & 0x80) != 0;
		header.opcode = data[0] & 0x0F;
		header.masked = (data[1] & 0x80) != 0;

		unsigned char stream_size = data[1] & 0x7F;

		switch (header.opcode)
		{
		case 0x00: // Continuation frame
			printf(__FUNCTION__ " - Continuation frame\n");
			break;
		case 0x01: // Text frame
			printf(__FUNCTION__ " - Text frame\n");
			break;
		case 0x02: // Binary frame
			printf(__FUNCTION__ " - Binary frame\n");
			break;
		case 0x08: // Connection close
			printf(__FUNCTION__ " - Connection close\n");
			break;
		case 0x09: // Ping
			printf(__FUNCTION__ " - Ping\n");
			break;
		case 0x0A: // Pong
			printf(__FUNCTION__ " - Pong\n");
			break;
		}

		if (stream_size <= 125) // WebSocketStreamSize::SmallStream
		{
			// Small stream
			printf(__FUNCTION__ " - Small stream\n");

			header.header_size = 6; // WebSocketHeaderSize::SmallHeader
			header.payload_size = stream_size;
			header.mask_offset = 2; // WebSocketMaskOffset::SmallOffset
		}
		else if (stream_size == 126) // WebSocketStreamSize::MediumStream
		{
			// Medium stream
			printf(__FUNCTION__ " - Medium stream\n");

			if (len >= 8)
			{
				header.header_size = 8; // WebSocketHeaderSize::MediumHeader
				unsigned short s = 0;
				memcpy(&s, data + 2, 2);
				header.payload_size = ntohs(s);
				header.mask_offset = 4; // WebSocketMaskOffset::MediumOffset
			}
			else
			{
				printf(__FUNCTION__ " - MediumStream header size is less than 8 bytes\n");
				pc->Disconnect();
				return FALSE;
			}
		}
		else if (stream_size == 127) // WebSocketStreamSize::BigStream
		{
			// Big stream
			printf(__FUNCTION__ " - Big stream\n");

			if (len >= 14)
			{
				header.header_size = 14; // WebSocketHeaderSize::BigHeader
				unsigned long long l = 0;
				memcpy(&l, data + 2, 8);
				header.payload_size = 0; // FIX THIS!
				header.mask_offset = 10; // WebSocketMaskOffset::BigOffset
			}
			else
			{
				printf(__FUNCTION__ " - BigStream header size is less than 14 bytes\n");
				pc->Disconnect();
				return FALSE;
			}
		}

		if (TRUE)
		{
			printf(__FUNCTION__ " - "
				"HeaderSize: %u\n"
				"MaskOffset: %d\n"
				"PayloadSize: %u\n"
				"fin: %d\n"
				"masked: %d\n"
				"opcode: %u\n"
				"res (%u, %u, %u)\n",
				header.header_size,
				header.mask_offset,
				header.payload_size,
				header.fin,
				header.masked,
				header.opcode,
				header.res[0],
				header.res[1],
				header.res[2]
				);
		}
	}
	else
	{
		printf(__FUNCTION__ " - Less than 6 bytes received, invalid header\n");
		pc->Disconnect();
		return FALSE;
	}

	return TRUE;
}

inline BOOL DecodeData(WebSocketStreamHeader& header, char* data, int len, char* output, size_t maxlen)
{
	char* ptr = data;
	char* end = data + 4 + len;

	char mask[4];
	memcpy(mask, data, sizeof(mask));
	ptr += sizeof(mask);

	char* out_ptr = output;
	char* out_end = output + maxlen;

	int i = 0;
	while (ptr < end)
	{
		if (out_ptr >= out_end)
			return FALSE;

		*out_ptr++ = *ptr++ ^ mask[i % sizeof(mask)];
		++i;
	}

	*out_ptr = 0;

	return TRUE;
}