#
# Cryptographic API
#

obj-$(CONFIG_CRYPTO) += api.o scatterwalk.o cipher.o digest.o compress.o

crypto_algapi-$(CONFIG_PROC_FS) += proc.o
crypto_algapi-objs := algapi.o $(crypto_algapi-y)
obj-$(CONFIG_CRYPTO_ALGAPI) += crypto_algapi.o

obj-$(CONFIG_CRYPTO_ABLKCIPHER) += ablkcipher.o
obj-$(CONFIG_CRYPTO_BLKCIPHER) += blkcipher.o

crypto_hash-objs := hash.o
obj-$(CONFIG_CRYPTO_HASH) += crypto_hash.o

obj-$(CONFIG_CRYPTO_MANAGER) += cryptomgr.o
obj-$(CONFIG_CRYPTO_HMAC) += hmac.o
obj-$(CONFIG_CRYPTO_XCBC) += xcbc.o
obj-$(CONFIG_CRYPTO_NULL) += crypto_null.o
obj-$(CONFIG_CRYPTO_MD4) += md4.o
obj-$(CONFIG_CRYPTO_MD5) += md5.o
obj-$(CONFIG_CRYPTO_SHA1) += sha1.o
obj-$(CONFIG_CRYPTO_SHA256) += sha256.o
obj-$(CONFIG_CRYPTO_SHA512) += sha512.o
obj-$(CONFIG_CRYPTO_WP512) += wp512.o
obj-$(CONFIG_CRYPTO_TGR192) += tgr192.o
obj-$(CONFIG_CRYPTO_GF128MUL) += gf128mul.o
obj-$(CONFIG_CRYPTO_ECB) += ecb.o
obj-$(CONFIG_CRYPTO_CBC) += cbc.o
obj-$(CONFIG_CRYPTO_PCBC) += pcbc.o
obj-$(CONFIG_CRYPTO_LRW) += lrw.o
obj-$(CONFIG_CRYPTO_CRYPTD) += cryptd.o
obj-$(CONFIG_CRYPTO_DES) += des.o
obj-$(CONFIG_CRYPTO_FCRYPT) += fcrypt.o
obj-$(CONFIG_CRYPTO_BLOWFISH) += blowfish.o
obj-$(CONFIG_CRYPTO_TWOFISH) += twofish.o
obj-$(CONFIG_CRYPTO_TWOFISH_COMMON) += twofish_common.o
obj-$(CONFIG_CRYPTO_SERPENT) += serpent.o
obj-$(CONFIG_CRYPTO_AES) += aes.o
obj-$(CONFIG_CRYPTO_CAMELLIA) += camellia.o
obj-$(CONFIG_CRYPTO_CAST5) += cast5.o
obj-$(CONFIG_CRYPTO_CAST6) += cast6.o
obj-$(CONFIG_CRYPTO_ARC4) += arc4.o
obj-$(CONFIG_CRYPTO_TEA) += tea.o
obj-$(CONFIG_CRYPTO_KHAZAD) += khazad.o
obj-$(CONFIG_CRYPTO_ANUBIS) += anubis.o
obj-$(CONFIG_CRYPTO_DEFLATE) += deflate.o
obj-$(CONFIG_CRYPTO_MICHAEL_MIC) += michael_mic.o
obj-$(CONFIG_CRYPTO_CRC32C) += crc32c.o

obj-$(CONFIG_CRYPTO_TEST) += tcrypt.o

#
# generic algorithms and the async_tx api
#
obj-$(CONFIG_XOR_BLOCKS) += xor.o
obj-$(CONFIG_ASYNC_CORE) += async_tx/
