# Credits

No code is adopted from another library. The package implements the standards below
from their text, and this page names what it takes from each. OpenSSL is the
benchmark's reference ([results](results.md)); when the package takes an idea from it
or another implementation, this page names it.

| Source | What the package takes |
|---|---|
| FIPS 180-4, *Secure Hash Standard* | SHA-1 and SHA-256: constants, initial values, rounds, and padding |
| RFC 2104, *HMAC* | HMAC's inner and outer pads, and hashing a key longer than a block |
| RFC 8018, *PKCS #5 v2.1*, §5.2 | PBKDF2: its blocks, their index, and the XOR of every iteration |
| NIST's *Examples with Intermediate Values* for FIPS 180 | SHA-1 and SHA-256 test vectors, the million-`a` input among them |
| RFC 4231 | HMAC-SHA-256 test cases 1 to 7 |
| RFC 7914, §11 | PBKDF2-HMAC-SHA-256 test vectors |
| RFC 6455, §1.3 | The WebSocket handshake example |

Two techniques are common practice rather than any one library's:

- The message schedule kept as a ring of 16 words, rather than expanded to 64 or 80
  first, as FIPS 180-4 §6.1.3 describes for SHA-1.
- In PBKDF2, the key's inner and outer HMAC states hashed once, and each later
  iteration run as two compression calls on copies of them, which RFC 2104 §4 suggests
  in its note on precomputing the pads.
