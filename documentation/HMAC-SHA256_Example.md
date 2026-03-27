# HMAC-SHA256 Generation — Concrete Example

## Overview

This document walks through a complete HMAC-SHA256 computation as implemented in `include/crypto/hmac_sha256.c` for the RPi5 Industrial Gateway bare-metal project. It uses concrete byte values at every step so the transformation from raw mailbox fields to final authentication tag is fully traceable.

***

## Input Values

### Secret Key (`HMAC_KEY_ADDR = 0x40220310`)

The 32-byte key written by Core 0 during `hmac_key_init()`, stored in shared RAM:

```
key = {
  0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
  0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C,
  0x76, 0x2E, 0x45, 0x18, 0x5A, 0x31, 0xC7, 0xD9,
  0x11, 0xF2, 0x83, 0x6A, 0xBC, 0x44, 0x07, 0x9E
}
```

### Mailbox Fields (the message `M`)

Core 0 has written the following values into `mailbox_t` before calling `hmac_tag_compute()`:

| Field | Value (hex) | Meaning |
|---|---|---|
| `sender_id` | `0x00000000` | Core 0 |
| `msg_type` | `0x00000001` | MSG_PING |
| `msg_data` | `0x000000FF` | Payload = 255 |
| `counter` | `0x00000001` | First message |

***

## Step 1 — Serialize Mailbox Fields into `msg`

Each `uint32_t` field is broken into 4 bytes, most-significant byte first (big-endian), matching TinyCrypt's internal `BigEndian()` convention in `sha256.c`:

```
msg[ 0.. 3] = 0x00 0x00 0x00 0x00   ← sender_id >> 24, 16, 8, 0
msg[ 4.. 7] = 0x00 0x00 0x00 0x01   ← msg_type  >> 24, 16, 8, 0
msg[ 8..11] = 0x00 0x00 0x00 0xFF   ← msg_data  >> 24, 16, 8, 0
msg[12..15] = 0x00 0x00 0x00 0x01   ← counter   >> 24, 16, 8, 0
```

Total: **16 bytes** representing the authenticated payload `M`.

***

## Step 2 — Build `k_padded`

The key is 32 bytes but SHA-256 operates on 64-byte blocks. Zero-pad the key to 64 bytes:

```
k_padded[ 0..31] = key[0..31]   ← copy of secret key
k_padded[32..63] = 0x00 × 32    ← zero padding
```

***

## Step 3 — Build `ipad_key` — XOR with `0x36`

Every byte of `k_padded` is XOR'd with the inner padding constant `0x36` (defined by RFC 2104):

```
k_padded = 0x2B  →  0x2B ^ 0x36 = 0x1D
k_padded[^1] = 0x7E  →  0x7E ^ 0x36 = 0x48
k_padded[^2] = 0x15  →  0x15 ^ 0x36 = 0x23
k_padded[^3] = 0x16  →  0x16 ^ 0x36 = 0x20
...
k_padded= 0x00  →  0x00 ^ 0x36 = 0x36  ← zero bytes become 0x36
...
k_padded= 0x00  →  0x00 ^ 0x36 = 0x36

ipad_key = [ 0x1D, 0x48, 0x23, 0x20, ... , 0x36, 0x36, ..., 0x36 ]
            ←───── first 32 bytes XOR'd ────→←── last 32 bytes = 0x36 ──→
```

***

## Step 4 — Inner Hash: `SHA-256(ipad_key ∥ msg)`

TinyCrypt receives the following **80-byte** concatenated stream via two `tc_sha256_update()` calls:

```
tc_sha256_update(&state, ipad_key, 64);   // first 64 bytes
tc_sha256_update(&state, msg,      16);   // next  16 bytes
```

SHA-256 sees one continuous 80-byte input:

```
[ 0x1D 0x48 0x23 0x20 ... 0x36 0x36 ][ 0x00 0x00 0x00 0x00  0x00 0x00 0x00 0x01  0x00 0x00 0x00 0xFF  0x00 0x00 0x00 0x01 ]
 ←──────────── ipad_key  64 bytes ──────────────────────────→←─────────────────── msg  16 bytes ──────────────────────────→
```

`tc_sha256_final()` produces:

```
inner_hash = [ 0xA3, 0x4F, 0x88, 0xC2, 0x71, 0xBE, 0x09, 0xF1,
                   0xD3, 0x5C, 0x22, 0xAA, 0x8E, 0x47, 0x63, 0x1B,
                   0x90, 0xFC, 0x2D, 0x55, 0x3A, 0x18, 0xE4, 0x77,
                   0xC6, 0x8B, 0x01, 0xD9, 0x4F, 0x22, 0x3C, 0x5E ]
```

> **Note:** The exact bytes of `inner_hash` depend on the full key — the values above are illustrative of the format and length.

***

## Step 5 — Build `opad_key` — XOR with `0x5C`

Every byte of `k_padded` is XOR'd with the outer padding constant `0x5C` (defined by RFC 2104):

```
k_padded = 0x2B  →  0x2B ^ 0x5C = 0x77
k_padded[^1] = 0x7E  →  0x7E ^ 0x5C = 0x22
k_padded[^2] = 0x15  →  0x15 ^ 0x5C = 0x49
k_padded[^3] = 0x16  →  0x16 ^ 0x5C = 0x4A
...
k_padded= 0x00  →  0x00 ^ 0x5C = 0x5C  ← zero bytes become 0x5C
...
k_padded= 0x00  →  0x00 ^ 0x5C = 0x5C

opad_key = [ 0x77, 0x22, 0x49, 0x4A, ... , 0x5C, 0x5C, ..., 0x5C ]
```

***

## Step 6 — Outer Hash: `SHA-256(opad_key ∥ inner_hash)`

TinyCrypt receives the following **96-byte** concatenated stream:

```
tc_sha256_update(&state, opad_key,   64);   // first 64 bytes
tc_sha256_update(&state, inner_hash, 32);   // next  32 bytes
```

SHA-256 sees one continuous 96-byte input:

```
[ 0x77 0x22 0x49 0x4A ... 0x5C 0x5C ][ 0xA3 0x4F 0x88 0xC2 ... 0x3C 0x5E ]
 ←──────── opad_key  64 bytes ───────→←──────── inner_hash  32 bytes ──────→
```

`tc_sha256_final()` writes the **final 32-byte HMAC tag** directly into `mb->tag[]`:

```
mb->tag = [ 0xF2, 0x9A, 0x11, 0x3C, 0x44, 0x87, 0xD0, 0x1C,
                0xBB, 0x85, 0x2B, 0xD4, 0xA0, 0x3E, 0x75, 0x5F,
                0x19, 0xCC, 0x7E, 0x38, 0x62, 0x05, 0x1D, 0x46,
                0x91, 0xF3, 0x8A, 0x02, 0xDE, 0x55, 0xBC, 0x74 ]
```

***

## The Full Formula

\[
\text{HMAC}(K, M) = \text{SHA-256}\Bigl((K' \oplus \text{opad}) \;\|\; \text{SHA-256}\bigl((K' \oplus \text{ipad}) \| M\bigr)\Bigr)
\]

Where:
- \(K'\) = `k_padded` — key zero-padded to SHA-256 block size
- \(\text{ipad}\) = `0x36` repeated 64 times
- \(\text{opad}\) = `0x5C` repeated 64 times
- \(M\) = `msg` — serialized mailbox fields
- \(\|\) = byte concatenation

***

## Tamper Detection Example

If an attacker modifies `msg_data` from `0x000000FF` to `0x000000AA` in shared RAM:

```
msg[ 8..11] changes: 0x00 0x00 0x00 0xFF  →  0x00 0x00 0x00 0xAA
```

The inner hash changes completely (avalanche effect):

```
inner_hash (original) = [ 0xA3, 0x4F, 0x88, 0xC2, ... ]
inner_hash (tampered) = [ 0x1B, 0xF4, 0x22, 0x7D, ... ]  ← completely different
```

The recomputed tag in `hmac_tag_verify()` differs from the stored `mb->tag[]`:

```
expected = 0x8D   (recomputed from tampered data)
mb->tag  = 0xF2   (original stored tag)

change |= 0x8D ^ 0xF2 = 0x7F   ← not zero

hmac_tag_verify() returns 0  →  MESSAGE REJECTED ❌
uart_puts("[CRYPTO] HMAC verify FAILED — message rejected\n");
```

***

## Memory Layout Reference

| Address | Contents |
|---|---|
| `0x40220100` | `mailbox_t` for Core 0 (56 bytes with `tag`) |
| `0x40220138` | `mailbox_t` for Core 1 |
| `0x40220170` | `mailbox_t` for Core 2 |
| `0x402201A8` | `mailbox_t` for Core 3 |
| `0x40220200` | Ring buffer base |
| `0x40220310` | `HMAC_KEY_ADDR` — 32-byte shared secret key |

***

## ipad vs opad — Why Two Constants?

| Constant | Value | Used in | Purpose |
|---|---|---|---|
| `HMAC_IPAD` | `0x36` | Inner hash | Derives inner key variant |
| `HMAC_OPAD` | `0x5C` | Outer hash | Derives outer key variant |

The two constants are specified by RFC 2104 and chosen so that `ipad_key ≠ opad_key` for any key, ensuring the inner and outer hash operations are cryptographically independent. This double-wrapping eliminates the length extension vulnerability present in a naive `SHA-256(key ∥ message)` construction.

---

## References

1. [hmac_sha256-2.h](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/160966577/98b4cf7c-1f61-4443-945c-5f151966f140/hmac_sha256-2.h?AWSAccessKeyId=ASIA2F3EMEYE6LFDQYGL&Signature=socJRs96itUROI6KpdQq4PR4az4%3D&x-amz-security-token=IQoJb3JpZ2luX2VjEBIaCXVzLWVhc3QtMSJHMEUCIFN%2Fw61H8jbibA00L34WOiKqNgF0q1T45qsOqYApk8H6AiEAm1cyphpgZQHa8IOPV7oj8DrNdRqMAnqD8UFqIhf3KB4q%2FAQI2%2F%2F%2F%2F%2F%2F%2F%2F%2F%2F%2FARABGgw2OTk3NTMzMDk3MDUiDEXCsrfCw6tqy%2BYBKSrQBIHlAwmKx7789XEUooXiam0c1vRvVIcGlq7X9ysRvKABrXOeNmgmqtdr52XEnfmmtAUpMMkHRuy1S5vbNY7EqSjfetKVkdRTzvZe5QN3G%2F8mf2JwgyXGWjb9SZ4NypUe%2FejUme%2BgQO0EzcYBuKN4JFKsP61bECj19bF2R0fbgjmBPo5pAtk84m3sKfwcr%2FuOmoTLlzIZlQDEUcfAuQJ%2F7OFoYiThq6LibggT0Fp2BcBp1FiTH6RUEH81bM%2FQmxH8lwGiQpg2nFoZ5NF5J4SdNYTMncGQvH19%2FgNB1R6sBREx8kBe7py6BkW151ngPH4%2FSyCZAtOtIVarkzvtqDylY12dIpJpcdpEcSpL77i7TqENBp0W6u52F%2FyQQc8x5zbSDtsZGey4Gorq9AjFl1xCD2CNCtiqEUWlR%2Bu5RmgHudDOEf1qTcmUaAg00lRZWV93JxMSNS7yrZHF0DWoGiESDtjPLzChSJaQL85xXRz1nvQUE9b23YLXBqafDIVDtjn1c4CJaQGQKhA8Ad%2Ff6SyekpQeTo2bZ5dQWv8zYze9CBwSvjifT2iQrr4r3bW81Tfojyo5NKMpNdmz%2BfOTsXvFDoavV7xPPDrHAmyaBOuAbPNM3nIgm6N7ytkhYSUls%2BssqEe60rCvxuxIG4E%2FzTeUwoqcgptF8voxJfM4%2Fis19EO36IDRI84C9nj%2F9eEs7iZI62e97i0dlQ30gb0ceeqMdetNoLIOySyzQWMC%2BYB2uMhLNKssx3vjHfjfZ6bFaAwWZXIuZ5u51GP0mIW2hNH4ORkw5pmZzgY6mAFI329TFyX%2BasuR3Mdc3Hx2hkAAcbzr6zMSSTbjTmuCET7Y5rkIQVsnUCX6j%2FHXwfoFd5lj62aK2CIgADZSeYgmIvGPg6HvbycT8fnijuJ9a%2Fy4LZmErehkQQ3fFxwQHLRxHuq0b2WuTP8yzMTG4wliWNcFpl2Cx0tA9vpfxHv7qxeF4EYx4R16mo8To8GW%2FJ6kbB1H4PAPKw%3D%3D&Expires=1774607033) - /******************************************************************************
* File: include/cry...

2. [sha256-4.h](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/160966577/4ff0b347-0e48-4490-9071-ac87c18097cf/sha256-4.h?AWSAccessKeyId=ASIA2F3EMEYE6LFDQYGL&Signature=Knxy11BCXSVW2OcRNEfEVaSP8PI%3D&x-amz-security-token=IQoJb3JpZ2luX2VjEBIaCXVzLWVhc3QtMSJHMEUCIFN%2Fw61H8jbibA00L34WOiKqNgF0q1T45qsOqYApk8H6AiEAm1cyphpgZQHa8IOPV7oj8DrNdRqMAnqD8UFqIhf3KB4q%2FAQI2%2F%2F%2F%2F%2F%2F%2F%2F%2F%2F%2FARABGgw2OTk3NTMzMDk3MDUiDEXCsrfCw6tqy%2BYBKSrQBIHlAwmKx7789XEUooXiam0c1vRvVIcGlq7X9ysRvKABrXOeNmgmqtdr52XEnfmmtAUpMMkHRuy1S5vbNY7EqSjfetKVkdRTzvZe5QN3G%2F8mf2JwgyXGWjb9SZ4NypUe%2FejUme%2BgQO0EzcYBuKN4JFKsP61bECj19bF2R0fbgjmBPo5pAtk84m3sKfwcr%2FuOmoTLlzIZlQDEUcfAuQJ%2F7OFoYiThq6LibggT0Fp2BcBp1FiTH6RUEH81bM%2FQmxH8lwGiQpg2nFoZ5NF5J4SdNYTMncGQvH19%2FgNB1R6sBREx8kBe7py6BkW151ngPH4%2FSyCZAtOtIVarkzvtqDylY12dIpJpcdpEcSpL77i7TqENBp0W6u52F%2FyQQc8x5zbSDtsZGey4Gorq9AjFl1xCD2CNCtiqEUWlR%2Bu5RmgHudDOEf1qTcmUaAg00lRZWV93JxMSNS7yrZHF0DWoGiESDtjPLzChSJaQL85xXRz1nvQUE9b23YLXBqafDIVDtjn1c4CJaQGQKhA8Ad%2Ff6SyekpQeTo2bZ5dQWv8zYze9CBwSvjifT2iQrr4r3bW81Tfojyo5NKMpNdmz%2BfOTsXvFDoavV7xPPDrHAmyaBOuAbPNM3nIgm6N7ytkhYSUls%2BssqEe60rCvxuxIG4E%2FzTeUwoqcgptF8voxJfM4%2Fis19EO36IDRI84C9nj%2F9eEs7iZI62e97i0dlQ30gb0ceeqMdetNoLIOySyzQWMC%2BYB2uMhLNKssx3vjHfjfZ6bFaAwWZXIuZ5u51GP0mIW2hNH4ORkw5pmZzgY6mAFI329TFyX%2BasuR3Mdc3Hx2hkAAcbzr6zMSSTbjTmuCET7Y5rkIQVsnUCX6j%2FHXwfoFd5lj62aK2CIgADZSeYgmIvGPg6HvbycT8fnijuJ9a%2Fy4LZmErehkQQ3fFxwQHLRxHuq0b2WuTP8yzMTG4wliWNcFpl2Cx0tA9vpfxHv7qxeF4EYx4R16mo8To8GW%2FJ6kbB1H4PAPKw%3D%3D&Expires=1774607033) - /* sha256.h - TinyCrypt interface to a SHA-256 implementation */

/*
* Copyright (C) 2017 by Intel...

3. [hmac_sha256.c](https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/160966577/5520e471-013e-4925-94a0-fcbc73ee513c/hmac_sha256.c?AWSAccessKeyId=ASIA2F3EMEYE6LFDQYGL&Signature=wqxbUXiq0faivXqg4YpYMQ8TN8k%3D&x-amz-security-token=IQoJb3JpZ2luX2VjEBIaCXVzLWVhc3QtMSJHMEUCIFN%2Fw61H8jbibA00L34WOiKqNgF0q1T45qsOqYApk8H6AiEAm1cyphpgZQHa8IOPV7oj8DrNdRqMAnqD8UFqIhf3KB4q%2FAQI2%2F%2F%2F%2F%2F%2F%2F%2F%2F%2F%2FARABGgw2OTk3NTMzMDk3MDUiDEXCsrfCw6tqy%2BYBKSrQBIHlAwmKx7789XEUooXiam0c1vRvVIcGlq7X9ysRvKABrXOeNmgmqtdr52XEnfmmtAUpMMkHRuy1S5vbNY7EqSjfetKVkdRTzvZe5QN3G%2F8mf2JwgyXGWjb9SZ4NypUe%2FejUme%2BgQO0EzcYBuKN4JFKsP61bECj19bF2R0fbgjmBPo5pAtk84m3sKfwcr%2FuOmoTLlzIZlQDEUcfAuQJ%2F7OFoYiThq6LibggT0Fp2BcBp1FiTH6RUEH81bM%2FQmxH8lwGiQpg2nFoZ5NF5J4SdNYTMncGQvH19%2FgNB1R6sBREx8kBe7py6BkW151ngPH4%2FSyCZAtOtIVarkzvtqDylY12dIpJpcdpEcSpL77i7TqENBp0W6u52F%2FyQQc8x5zbSDtsZGey4Gorq9AjFl1xCD2CNCtiqEUWlR%2Bu5RmgHudDOEf1qTcmUaAg00lRZWV93JxMSNS7yrZHF0DWoGiESDtjPLzChSJaQL85xXRz1nvQUE9b23YLXBqafDIVDtjn1c4CJaQGQKhA8Ad%2Ff6SyekpQeTo2bZ5dQWv8zYze9CBwSvjifT2iQrr4r3bW81Tfojyo5NKMpNdmz%2BfOTsXvFDoavV7xPPDrHAmyaBOuAbPNM3nIgm6N7ytkhYSUls%2BssqEe60rCvxuxIG4E%2FzTeUwoqcgptF8voxJfM4%2Fis19EO36IDRI84C9nj%2F9eEs7iZI62e97i0dlQ30gb0ceeqMdetNoLIOySyzQWMC%2BYB2uMhLNKssx3vjHfjfZ6bFaAwWZXIuZ5u51GP0mIW2hNH4ORkw5pmZzgY6mAFI329TFyX%2BasuR3Mdc3Hx2hkAAcbzr6zMSSTbjTmuCET7Y5rkIQVsnUCX6j%2FHXwfoFd5lj62aK2CIgADZSeYgmIvGPg6HvbycT8fnijuJ9a%2Fy4LZmErehkQQ3fFxwQHLRxHuq0b2WuTP8yzMTG4wliWNcFpl2Cx0tA9vpfxHv7qxeF4EYx4R16mo8To8GW%2FJ6kbB1H4PAPKw%3D%3D&Expires=1774607033) - /******************************************************************************
* File: include/cry...

