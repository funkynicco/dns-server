#pragma once

/***************************************************************************************
 * General
 ***************************************************************************************/

// Log all socket pool allocations
//#define __LOG_SOCKET_ALLOCATIONS

/***************************************************************************************
 * DNS Server
 ***************************************************************************************/
// Log all allocations done
//#define __LOG_DNS_ALLOCATIONS // temp

// Log server I/O related
//#define __LOG_DNS_SERVER_IO // temp

// Log requests
#define __LOG_DNS_REQUESTS

/***************************************************************************************
 * Proxy Server
 ***************************************************************************************/

/***************************************************************************************
 * Web Server
 ***************************************************************************************/
// Log all allocations partaining to web server
//#define __LOG_WEB_ALLOCATIONS

 // Log web server I/O related
//#define __LOG_WEB_SERVER_IO

 // Log web requests
//#define __LOG_WEB_REQUESTS