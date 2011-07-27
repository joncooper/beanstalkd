
## Resources

[SASL Application Programmer's Guide](http://www.cyrusimap.org/docs/cyrus-sasl/2.1.23/programming.php)
Oracle [Introduction to SASL](http://download.oracle.com/docs/cd/E19963-01/html/819-2145/sasl.intro.20.html)
memcached SASL support [commit](https://github.com/memcached/memcached/commit/f1307c4d9cadb94076a99cc2f88a00f7e0b4161f)
memcached [SASL Auth Protocol](http://code.google.com/p/memcached/wiki/SASLAuthProtocol)
libmemcached SASL support [commit](http://bazaar.launchpad.net/~trond-norbye/libmemcached/sasl/revision/802#libmemcached/sasl.c)
[Surviving Cyrus SASL](http://postfix.state-of-mind.de/patrick.koetter/surviving_cyrus_sasl.pdf) (PDF)

## Notes

- To use SASL, a protocol includes a command for identifying and authenticating a user to a server. (and for optionally negotiating a security layer for subsequent protocol interaction.)

- SASL mechanisms are named by strings, from 1-20 characters in length, consisting of upper-case letters, digits, hyphens, and/or underscores. SASL mechanism names must be registered with IANA.

- If a server supports the requested mechanism, it initiates a challenge/response protocol exchange. These challenges and responses are defined as binary tokens of arbitrary length.

- The protocol defines how the binary tokens are encoded for transfer over its connection.


// Lifecycle:
//   first, call sasl_server_init
//   then, on each new connection, call sasl_server_new at accept() time
//   get a list of supported mechanisms with sasl_listmech
//   when the client requests to authenticate, sasl_server_start, then
//      repeatedly hit sasl_server_step
//      you'll get SASL_INTERACT, SASL_OK or SASL_CONTINUE; 
//         anything else means an error.
//   when the connection is concluded, make a call to sasl_dispose
//   finish by calling sasl_done (on global shutdown)

## Protocol Definitions

Limits:

MAX_SASL_MECH_LEN 32
_
Requests:

CMD_AUTH_LIST_MECHANISMS - request a list of supported auth mechanisms
   auth-list-mechanisms\r\n
CMD_AUTH_START - begin an authentication request
   auth-start <mechanism> <bytes>\r\n
   <auth data>\r\n
CMD_AUTH_STEP - respond to a MSG_AUTH_CONTINUE
   auth-step <bytes>\r\n
   <auth data>\r\n
  
<bytes> excludes \r\n and refers to the size of the <auth data> payload

Responses:

MSG_AUTH_OK
MSG_AUTH_UNAUTHORIZED - an authentication or authorization failure occurred.
MSG_AUTH_CONTINUE - another authorization step is required
MSG_UNKNOWN_COMMAND - server doesn't know what you're talking about and is probably a beanstalkd

reply_msg seems to be not resetting input buffers correctly_




ensure your config has the right dc set up
http://www.danbishop.org/2011/05/01/ubuntu-11-04-sbs-small-business-server-setup-part-3-openldap/

install apt-get libnss-ldapd libpam-ldapd

