
## Resources

[SASL Application Programmer's Guide](http://www.cyrusimap.org/docs/cyrus-sasl/2.1.23/programming.php)
Oracle [Introduction to SASL](http://download.oracle.com/docs/cd/E19963-01/html/819-2145/sasl.intro.20.html)

## Notes

- To use SASL, a protocol includes a command for identifying and authenticating a user to a server. (and for optionally negotiating a security layer for subsequent protocol interaction.)

- SASL mechanisms are named by strings, from 1-20 characters in length, consisting of upper-case letters, digits, hyphens, and/or underscores. SASL mechanism names must be registered with IANA.

- If a server supports the requested mechanism, it initiates a challenge/response protocol exchange. These challenges and responses are defined as binary tokens of arbitrary length.

- The protocol defines how the binary tokens are encoded for transfer over its connection.


// See http://www.cyrusimap.org/docs/cyrus-sasl/2.1.23/programming.php

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
