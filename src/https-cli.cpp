/*
 * Part of Rngcoin project
 *
 * This code is based on the examples from 
 * https://github.com/libevent/libevent/tree/master/sample
 * LICENSE: BSD 3-caluse license
 *
 * All HTTPS funcions are merged into a single file
 */

/*
  This is an example of how to hook up evhttp with bufferevent_ssl

  It just GETs an https URL given on the command-line and prints the response
  body to stdout.

  Actually, it also accepts plain http URLs to make it easy to compare http vs
  https code paths.

  Loosely based on le-proxy.c.
 */

#define CURL_HOST_NOMATCH 0
#define CURL_HOST_MATCH   1

// Get rid of OSX 10.7 and greater deprecation warnings.
#if defined(__APPLE__) && defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#define snprintf _snprintf
#define strcasecmp _stricmp 
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/http.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <ctype.h>

#include "clientversion.h"

#include <map>
//--------------- Body of "openssl_hostname_validation.h"

typedef enum {
        MatchFound,
        MatchNotFound,
        NoSANPresent,
        MalformedCertificate,
        Error
} HostnameValidationResult;


//--------------- Body of "hostcheck.c"

// RNG works on Intel platform only, there is always ASCII
static char Curl_raw_toupper(char in) {
  return toupper(in);
}

/*
 * Curl_raw_equal() is for doing "raw" case insensitive strings. This is meant
 * to be locale independent and only compare strings we know are safe for
 * this.  See http://daniel.haxx.se/blog/2008/10/15/strcasecmp-in-turkish/ for
 * some further explanation to why this function is necessary.
 *
 * The function is capable of comparing a-z case insensitively even for
 * non-ascii.
 */

static int Curl_raw_equal(const char *first, const char *second)
{
  while(*first && *second) {
    if(Curl_raw_toupper(*first) != Curl_raw_toupper(*second))
      /* get out of the loop as soon as they don't match */
      break;
    first++;
    second++;
  }
  /* we do the comparison here (possibly again), just to make sure that if the
     loop above is skipped because one of the strings reached zero, we must not
     return this as a successful match */
  return (Curl_raw_toupper(*first) == Curl_raw_toupper(*second));
}

static int Curl_raw_nequal(const char *first, const char *second, size_t max)
{
  while(*first && *second && max) {
    if(Curl_raw_toupper(*first) != Curl_raw_toupper(*second)) {
      break;
    }
    max--;
    first++;
    second++;
  }
  if(0 == max)
    return 1; /* they are equal this far */

  return Curl_raw_toupper(*first) == Curl_raw_toupper(*second);
}

/*
 * Match a hostname against a wildcard pattern.
 * E.g.
 *  "foo.host.com" matches "*.host.com".
 *
 * We use the matching rule described in RFC6125, section 6.4.3.
 * http://tools.ietf.org/html/rfc6125#section-6.4.3
 */

static int hostmatch(const char *hostname, const char *pattern)
{
  const char *pattern_label_end, *pattern_wildcard, *hostname_label_end;
  int wildcard_enabled;
  size_t prefixlen, suffixlen;
  pattern_wildcard = strchr(pattern, '*');
  if(pattern_wildcard == NULL)
    return Curl_raw_equal(pattern, hostname) ?
      CURL_HOST_MATCH : CURL_HOST_NOMATCH;

  /* We require at least 2 dots in pattern to avoid too wide wildcard
     match. */
  wildcard_enabled = 1;
  pattern_label_end = strchr(pattern, '.');
  if(pattern_label_end == NULL || strchr(pattern_label_end+1, '.') == NULL ||
     pattern_wildcard > pattern_label_end ||
     Curl_raw_nequal(pattern, "xn--", 4)) {
    wildcard_enabled = 0;
  }
  if(!wildcard_enabled)
    return Curl_raw_equal(pattern, hostname) ?
      CURL_HOST_MATCH : CURL_HOST_NOMATCH;

  hostname_label_end = strchr(hostname, '.');
  if(hostname_label_end == NULL ||
     !Curl_raw_equal(pattern_label_end, hostname_label_end))
    return CURL_HOST_NOMATCH;

  /* The wildcard must match at least one character, so the left-most
     label of the hostname is at least as large as the left-most label
     of the pattern. */
  if(hostname_label_end - hostname < pattern_label_end - pattern)
    return CURL_HOST_NOMATCH;

  prefixlen = pattern_wildcard - pattern;
  suffixlen = pattern_label_end - (pattern_wildcard+1);
  return Curl_raw_nequal(pattern, hostname, prefixlen) &&
    Curl_raw_nequal(pattern_wildcard+1, hostname_label_end - suffixlen,
                    suffixlen) ?
    CURL_HOST_MATCH : CURL_HOST_NOMATCH;
}

int Curl_cert_hostcheck(const char *match_pattern, const char *hostname)
{
  if(!match_pattern || !*match_pattern ||
      !hostname || !*hostname) /* sanity check */
    return 0;

  if(Curl_raw_equal(hostname, match_pattern)) /* trivial case */
    return 1;

  if(hostmatch(hostname,match_pattern) == CURL_HOST_MATCH)
    return 1;
  return 0;
}


//--------------- Body of "openssl_hostname_validation.c"

#include <openssl/x509v3.h>
#include <openssl/ssl.h>

#define HOSTNAME_MAX_SIZE 255

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
#define ASN1_STRING_get0_data ASN1_STRING_data
#endif

/**
* Tries to find a match for hostname in the certificate's Common Name field.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if the Common Name had a NUL character embedded in it.
* Returns Error if the Common Name could not be extracted.
*/
static HostnameValidationResult matches_common_name(const char *hostname, const X509 *server_cert) {
        int common_name_loc = -1;
        X509_NAME_ENTRY *common_name_entry = NULL;
        ASN1_STRING *common_name_asn1 = NULL;
        const char *common_name_str = NULL;

        // Find the position of the CN field in the Subject field of the certificate
        common_name_loc = X509_NAME_get_index_by_NID(X509_get_subject_name((X509 *) server_cert), NID_commonName, -1);
        if (common_name_loc < 0) {
                return Error;
        }

        // Extract the CN field
        common_name_entry = X509_NAME_get_entry(X509_get_subject_name((X509 *) server_cert), common_name_loc);
        if (common_name_entry == NULL) {
                return Error;
        }

        // Convert the CN field to a C string
        common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
        if (common_name_asn1 == NULL) {
                return Error;
        }
        common_name_str = (char *) ASN1_STRING_get0_data(common_name_asn1);

        // Make sure there isn't an embedded NUL character in the CN
        if ((size_t)ASN1_STRING_length(common_name_asn1) != strlen(common_name_str)) {
                return MalformedCertificate;
        }

        // Compare expected hostname with the CN
        if (Curl_cert_hostcheck(common_name_str, hostname) == CURL_HOST_MATCH) {
                return MatchFound;
        }
        else {
                return MatchNotFound;
        }
}

/**
* Tries to find a match for hostname in the certificate's Subject Alternative Name extension.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
* Returns NoSANPresent if the SAN extension was not present in the certificate.
*/
static HostnameValidationResult matches_subject_alternative_name(const char *hostname, const X509 *server_cert) {
        HostnameValidationResult result = MatchNotFound;
        int i;
        int san_names_nb = -1;

        // Try to extract the names within the SAN extension from the certificate
        STACK_OF(GENERAL_NAME) *san_names = (STACK_OF(GENERAL_NAME) *)X509_get_ext_d2i((X509 *) server_cert, NID_subject_alt_name, NULL, NULL);
        if (san_names == NULL) {
                return NoSANPresent;
        }
        san_names_nb = sk_GENERAL_NAME_num(san_names);

        // Check each name within the extension
        for (i=0; i<san_names_nb; i++) {
                const GENERAL_NAME *current_name = sk_GENERAL_NAME_value(san_names, i);

                if (current_name->type == GEN_DNS) {
                        // Current name is a DNS name, let's check it
                        const char *dns_name = (char *) ASN1_STRING_get0_data(current_name->d.dNSName);

                        // Make sure there isn't an embedded NUL character in the DNS name
                        if ((size_t)ASN1_STRING_length(current_name->d.dNSName) != strlen(dns_name)) {
                                result = MalformedCertificate;
                                break;
                        }
                        else { // Compare expected hostname with the DNS name
                                if (Curl_cert_hostcheck(dns_name, hostname)
                                    == CURL_HOST_MATCH) {
                                        result = MatchFound;
                                        break;
                                }
                        }
                }
        }
        sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);

        return result;
}


/**
* Validates the server's identity by looking for the expected hostname in the
* server's certificate. As described in RFC 6125, it first tries to find a match
* in the Subject Alternative Name extension. If the extension is not present in
* the certificate, it checks the Common Name instead.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
* Returns Error if there was an error.
*/
HostnameValidationResult validate_hostname(const char *hostname, const X509 *server_cert) {
        HostnameValidationResult result;

        if((hostname == NULL) || (server_cert == NULL))
                return Error;

        // First try the Subject Alternative Names extension
        result = matches_subject_alternative_name(hostname, server_cert);
        if (result == NoSANPresent) {
                // Extension was not found: try the Common Name
                result = matches_common_name(hostname, server_cert);
        }

        return result;
}


//--------------- https-client here --------------

static struct event_base *base;
static int ignore_cert = 0;

// Global variables for return - used, since long chain of calls/callbacks
static std::string s_rc_str;
static std::string s_rc_err;
static int s_http_rc;

static void
http_request_done(struct evhttp_request *req, void *ctx)
{
	char buffer[1024];
	int nread;

	if (req == NULL) {
		/* If req is NULL, it means an error occurred, but
		 * sadly we are mostly left guessing what the error
		 * might have been.  We'll do our best... */
		struct bufferevent *bev = (struct bufferevent *) ctx;
		unsigned long oslerr;
		int errcode = EVUTIL_SOCKET_ERROR();
		sprintf(buffer, "https: Socket Err=%d (%s)\n", errcode, evutil_socket_error_to_string(errcode));
		s_rc_err.append(buffer);
		/* Print out the OpenSSL error queue that libevent
		 * squirreled away for us, if any. */
		while ((oslerr = bufferevent_get_openssl_error(bev))) {
			ERR_error_string_n(oslerr, buffer, sizeof(buffer));
			s_rc_err.append("\tOpenSSL Err=");
			s_rc_err.append(buffer);
			s_rc_err.append("\n");
		}
		return;
	}

	if((s_http_rc = evhttp_request_get_response_code(req)) > 300) {
		sprintf(buffer, "https: HTTP Err=%u (%s)\n", 
			evhttp_request_get_response_code(req),
			evhttp_request_get_response_code_line(req));
		s_rc_err.append(buffer);
		return;
	}

	while ((nread = evbuffer_remove(evhttp_request_get_input_buffer(req),
		    buffer, sizeof(buffer)))
	       > 0) {
		/* These are just arbitrary chunks of 1024 bytes.
		 * They are not lines, so we can't treat them as such. */
		s_rc_str.append(buffer, nread);
	}
}

/* See http://archives.seul.org/libevent/users/Jan-2013/msg00039.html */
static int cert_verify_callback(X509_STORE_CTX *x509_ctx, void *arg)
{
	char cert_str[256];
	const char *host = (const char *) arg;
	const char *res_str = "X509_verify_cert failed";
	HostnameValidationResult res = Error;

	/* This is the function that OpenSSL would call if we hadn't called
	 * SSL_CTX_set_cert_verify_callback().  Therefore, we are "wrapping"
	 * the default functionality, rather than replacing it. */
	int ok_so_far = 0;

	X509 *server_cert = NULL;

	if (ignore_cert) {
		return 1;
	}

	ok_so_far = X509_verify_cert(x509_ctx);

	server_cert = X509_STORE_CTX_get_current_cert(x509_ctx);

	if (ok_so_far) {
		res = validate_hostname(host, server_cert);

		switch (res) {
		case MatchFound:
			res_str = "MatchFound";
			break;
		case MatchNotFound:
			res_str = "MatchNotFound";
			break;
		case NoSANPresent:
			res_str = "NoSANPresent";
			break;
		case MalformedCertificate:
			res_str = "MalformedCertificate";
			break;
		case Error:
			res_str = "Error";
			break;
		default:
			res_str = "WTF!";
			break;
		}
	}

	X509_NAME_oneline(X509_get_subject_name (server_cert),
			  cert_str, sizeof (cert_str));

	if (res == MatchFound) {
		return 1;
	} else {
		char buffer[1024];
		sprintf(buffer, "SSL: Cert Err=%s (%s:%s)\n", 
		       res_str, host, cert_str);
		s_rc_err.append(buffer);
		return 0;
	}
}


// https request using libevent.
// ATTN: Function is NOT REENTERABLE
// Returns HTTP status code if OK, or -1 if error
// Ret contains server answer (if OK), orr error text (-1)
int
HttpsLE(const char *host, const char *get, const char *post, const std::map<std::string,std::string> &header, std::string *ret) {

  s_rc_str.clear();
  s_rc_err.clear();
  s_http_rc = 0;

  SSL_CTX *ssl_ctx = NULL;
  SSL *ssl = NULL;
  struct bufferevent *bev;
  struct evhttp_request *req;
  struct evkeyvalq *output_headers;
  struct evbuffer *output_buffer;

  struct evhttp_connection *evcon = NULL;
  char buf[1024];
  int err;
  buf[0] = 0;
  char *tmp = buf + 4; // TMP char array for printfs

  do {
#ifdef _WIN32
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
      sprintf(buf, "HttpsLE: WSAStartup failed with error: %d\n", err);
      break;
    }
#endif // _WIN32

    if(get == NULL || *get == 0)
      get = "/";

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
    // Initialize OpenSSL
    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif

    /* Create a new OpenSSL context */
    ssl_ctx = SSL_CTX_new(SSLv23_method());
    if (!ssl_ctx) {
      strcpy(buf, "HttpsLE: Err=SSL_CTX_new\n");
      break;
    }

    X509_STORE *store = SSL_CTX_get_cert_store(ssl_ctx);

#ifndef _WIN32
    /* Attempt to use the system's trusted root certificates. */
    if (X509_STORE_set_default_paths(store) != 1) {
      strcpy(buf, "HttpsLE: Err=X509_STORE_set_default_paths\n");
      break;
    }	
#else // _WIN32
    // Got sample from
    // http://openssl.6102.n7.nabble.com/Get-root-certificates-from-System-Store-of-Windows-td40959.html
    // another: https://stackoverflow.com/questions/9507184/can-openssl-on-windows-use-the-system-certificate-store
    HCERTSTORE hStore = CertOpenSystemStore(0, "ROOT");
    if(!hStore) {
      strcpy(buf, "HttpsLE: Err=CertOpenSystemStore\n");
      break;
    }

    PCCERT_CONTEXT pContext;
    while (pContext = CertEnumCertificatesInStore(hStore, pContext)) {
      const unsigned char *buf = pContext->pbCertEncoded;
      X509 *x509 = d2i_X509(NULL, &buf, pContext->cbCertEncoded);
      if (x509) {
        X509_STORE_add_cert(store, x509);
        X509_free(x509);
      }
    }
    CertFreeCertificateContext(pContext);
    CertCloseStore(hStore, 0);
#endif // _WIN32


	/* Ask OpenSSL to verify the server certificate.  Note that this
	 * does NOT include verifying that the hostname is correct.
	 * So, by itself, this means anyone with any legitimate
	 * CA-issued certificate for any website, can impersonate any
	 * other website in the world.  This is not good.  See "The
	 * Most Dangerous Code in the World" article at
	 * https://crypto.stanford.edu/~dabo/pubs/abstracts/ssl-client-bugs.html
	 */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
	/* This is how we solve the problem mentioned in the previous
	 * comment.  We "wrap" OpenSSL's validation routine in our
	 * own routine, which also validates the hostname by calling
	 * the code provided by iSECPartners.  Note that even though
	 * the "Everything You've Always Wanted to Know About
	 * Certificate Validation With OpenSSL (But Were Afraid to
	 * Ask)" paper from iSECPartners says very explicitly not to
	 * call SSL_CTX_set_cert_verify_callback (at the bottom of
	 * page 2), what we're doing here is safe because our
	 * cert_verify_callback() calls X509_verify_cert(), which is
	 * OpenSSL's built-in routine which would have been called if
	 * we hadn't set the callback.  Therefore, we're just
	 * "wrapping" OpenSSL's routine, not replacing it. */
    SSL_CTX_set_cert_verify_callback(ssl_ctx, cert_verify_callback,
					  (void *) host);
    // Create event base
    base = event_base_new();
    if(!base) {
      strcpy(buf, "HttpsLE: Err=event_base_new()\n");
      break;
    }
    // Create OpenSSL bufferevent and stack evhttp on top of it
    ssl = SSL_new(ssl_ctx);
    if (ssl == NULL) {
      strcpy(buf, "HttpsLE: Err=SSL_new()\n");
      break;
    }
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	// Set hostname for SNI extension
	SSL_set_tlsext_host_name(ssl, host);
#endif

    bev = bufferevent_openssl_socket_new(base, -1, ssl,
			BUFFEREVENT_SSL_CONNECTING,
			BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

    if (bev == NULL) {
      strcpy(buf, "HttpsLE: Err=bufferevent_openssl_socket_new() failed\n");
      break;
    }

    bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);

    // For simplicity, we let DNS resolution block. Everything else should be
    // asynchronous though.
    evcon = evhttp_connection_base_bufferevent_new(base, NULL, bev,
		host, 443);
    if (evcon == NULL) {
      strcpy(buf, "HttpsLE: Err=evhttp_connection_base_bufferevent_new() failed\n");
      break;
    }

    evhttp_connection_set_timeout(evcon, 20);

    // Fire off the request
    req = evhttp_request_new(http_request_done, bev);
    if (req == NULL) {
      strcpy(buf, "HttpsLE: Err=evhttp_request_new() failed\n");
      break;
    }

    // Create HTTP heeader
    output_headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(output_headers, "Host", host);
    sprintf(tmp, "rngcoin-json-rpc/%s", FormatFullVersion().c_str());
    evhttp_add_header(output_headers, "User-Agent", tmp);
    evhttp_add_header(output_headers, "Accept", "application/json");
    evhttp_add_header(output_headers, "Connection", "close");


    if(post) {
      size_t len = strlen(post);
      output_buffer = evhttp_request_get_output_buffer(req);
      evbuffer_add(output_buffer, post, len);
      evhttp_add_header(output_headers, "Content-Type", "application/json");
      evutil_snprintf(tmp, sizeof(buf)-8, "%zu", len);
      evhttp_add_header(output_headers, "Content-Length", tmp);
    }

    // Additional header fields from client
    for(std::map<std::string,std::string>::const_iterator it = header.begin(); it != header.end(); ++it)
      evhttp_add_header(output_headers, it->first.c_str(), it->second.c_str());

    err = evhttp_make_request(evcon, req, post? EVHTTP_REQ_POST : EVHTTP_REQ_GET, get);
    if (err != 0) {
      strcpy(buf, "HttpsLE: Err=evhttp_make_request() failed\n\n");
      break;
    }

    event_base_dispatch(base);

  } while(false);

  if(buf[0]) 
    s_rc_err.append(buf);

  // Cleanup
  if (evcon)
    evhttp_connection_free(evcon);

  event_base_free(base);

  if (ssl_ctx)
    SSL_CTX_free(ssl_ctx);

  //?? maybe, need release SSL_free(ssl);

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
    EVP_cleanup();
    ERR_free_strings();

#if OPENSSL_VERSION_NUMBER < 0x10000000L
    ERR_remove_state(0);
#else
    ERR_remove_thread_state(NULL);
#endif
    CRYPTO_cleanup_all_ex_data();
    sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
#endif /* (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER) */

#ifdef _WIN32
	WSACleanup();
#endif

  if(s_rc_err.empty()) {
    *ret = s_rc_str;
    return s_http_rc;
  } else {
    *ret = s_rc_err;
    return (s_http_rc > 0)? -s_http_rc : -1;
  }

}

//----------------------------E-O-F--------------------------------------

