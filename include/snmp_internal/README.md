The netsnmp library (http://www.net-snmp.org) provides header files that tend to pollute the
global namespace - for example, they define READ and WRITE, breaking any other code that tries to use those
(e.g. in an enum).

To minimise the effect of this, we've split our SNMP code into two sections:

* 'public' headers, which define an abstract interface to a table and a factory method for creating
    it, and do not include net-snmp headers. Client code (like Sprout's main.cpp) should only
    include these headers.
* These 'internal' headers, which define implementations of those abstract interfaces, and do
    reference netsnmp objects and include these headers. These headers should not be included
    directly outside of cpp-common code.

This separation is enforced by clearwater-fv-test, which has a compile-time error if READ is defined
after including only the public headers.
