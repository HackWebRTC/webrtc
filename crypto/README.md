## What should be in the crypto directory
The crypto directory is intended to be a centralized location for all
cryptography code in WebRTC. This includes DTLS-SRTP, SRTP, HTTPS,
TLS, cryptography primitives interfaces such as HKDF and secure random
(backed by a concrete implementation in BoringSSL).

## What should not be in the crypto directory
The crypto directory is not a general purpose security directory. Security
utilities such as ZeroBufferOnFree should still go in rtc_base/ and fuzzing
utilities in test/fuzzers. ASAN, TSAN and MSAN sanatizer utilities should also
not be in this directory.
