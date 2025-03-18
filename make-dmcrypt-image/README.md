# make-dmcrypt-image

Tool to create dm-crypt compatible disk (filesystem) images

Copyright (c) Fixposition AG <https://www.fixposition.com>

License: see [`LICENSE`](./LICENSE)

## Build

```sh
sudo apt install kcapi-tools libkcapi-dev
make
```

## Use

For example, to encrypt the unencrypted disk (filesystem) image `unecrypted.img` using "aes-cbc-essiv:sha256" to an
encrypted image `encrypted.img` do:

```sh
dd if=/dev/urandom bs=1 count=16 > secret.key # 128 bits
./make-dmcrypt-image -s 2 -i unencrypted.img -o encrypted.img -k secret.key
```

## Background

Everything is explained here:

- https://en.wikipedia.org/wiki/Disk_encryption_theory
- https://en.wikipedia.org/wiki/Advanced_Encryption_Standard


Using standard Linux tools (losetup, dmsetup) we can encrypt a filesystem image like this:

```sh
# Make a unencrypted fs image
dd if=/dev/zero of=unencrypted.img bs=512 count=2048 # 1 MiB
sudo losetup /dev/loop0 unencrypted.img
sudo mke2fs /dev/loop0
# optionally, copy some data into the image
# mount /dev/loop0 /mnt/tmp
# rsync -rav /some/data/ /mnt/tmp/
# umount /mnt/tmp
sudo losetup -d /dev/loop0

# Make a aes-cbc-essiv:sha256 encrypted copy of that
dd if=/dev/urandom bs=1 count=16 >encrypted_essiv.key # 128 bits
dd if=/dev/zero of=encrypted_essiv.img bs=512 count=2048
sudo losetup /dev/loop0 encrypted_essiv.img
sudo dmsetup create gugus --table "0 2048 crypt aes-cbc-essiv:sha256 $(cat encrypted_essiv.key | xxd -p | tr -d '\n') 0 /dev/loop0 0"
sudo dd if=unencrypted.img of=/dev/mapper/gugus
sudo sync
sudo dmsetup remove gugus
sudo losetup -d /dev/loop0

# Make a aes-xts-plain encrypted version
dd if=/dev/urandom bs=1 count=32 >encvrtk2.key # 256 bits
dd if=/dev/zero of=encrypted_plain.img bs=512 count=2048
sudo losetup /dev/loop0 encrypted_plain.img
sudo dmsetup create gugus --table "0 2048 crypt aes-xts-plain $(cat encrypted_plain.key | xxd -p | tr -d '\n') 0 /dev/loop0 0"
sudo dd if=unencrypted.img of=/dev/mapper/gugus
sudo sync
sudo dmsetup remove gugus
sudo losetup -d /dev/loop0
```

However, this is a bit silly and has several problems: We have to use `sudo` and a loop device, which may not be
available or desirable, or even possible, in some scenarios (CI jobs, for example).

We can do the same using the make-dmcrypt-image tool, which doesn't need sudo or a loop device:

```sh
# - scheme 1: aes-xts-plain
./make-dmcrypt-image -s 1 -i unencrypted.img -o encrypted_plain_nosudo.img -k encrypted_plain.key
md5sum encrypted_plain.img encrypted_plain_nosudo.img
#   da44c97c0c99383dba0be0f11f188650  encrypted_plain.img
#   da44c97c0c99383dba0be0f11f188650  encrypted_plain_nosudo.img     Yay, same MD5! :-)

# - scheme 2 aes-cbc-essiv:sha256
./make-dmcrypt-image -s 2 -i unencrypted.img -o encrypted_essiv_nosudo.img -k encrypted.key
md5sum encrypted.img encrypted_essiv_nosudo.img
#   e952a4952e7a441314f359d141cac480  encrypted_essiv.img
#   e952a4952e7a441314f359d141cac480  encrypted_essiv_nosudo.img     Yay, same MD5! :-)
```
