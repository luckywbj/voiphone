# Introduction #

This is to help one build pjsip 1.0 on OS X 10.5.5 with the iPhone SDK 2.x using the sound code from the [siphon](http://code.google.com/p/siphon/) project.

# Note #

_**ipodsound.c** does not work with the 3.0 SDK beta. The sound api probably has changes and the source will need to be ported._

To have this work with other versions of the iPhone SDK (maybe) try changing the occurrences of /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS **2.1** .sdk to whichever version you have downloaded.

i.e. /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS **2.2** .sdk

# Details #

Put the ipodsound.c file file into pjmedia/source/pjmedia directory. You can get a copy from the downloads section of this site.

Set the path to include the arm tools in the SDK
```
export PATH=$PATH:/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/
```

Setup symbolic links for gcc like cross compile

```
cd /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/
ln -s ar arm-apple-darwin9-ar
ln -s cpp arm-apple-darwin9-cpp 
ln -s arm-apple-darwin9-g++-4.0.1 arm-apple-darwin9-g++
ln -s arm-apple-darwin9-gcc-4.0.1 arm-apple-darwin9-gcc
ln -s ld arm-apple-darwin9-ld
ln -s ranlib arm-apple-darwin9-ranlib
```

Edit aconfigure.ac and add this at line 398:

```
arm-apple-darwin*)
        LIBS="$LIBS -framework CoreAudio -framework CoreFoundation -framework AudioToolbox"
        ac_pjmedia_snd=ipod
        AC_MSG_RESULT([Checking sound device backend... AudioQueue])
        ;;
```

Edit build/cc-gcc.mak

```
export RANLIB = $(CROSS_COMPILE)ranlib -o
```

Edit the following build/rules.mak

```
$(LIB): $(OBJDIRS) $(OBJS) $($(APP)_EXTRA_DEP)
	if test ! -d $(LIBDIR); then $(subst @@,$(subst /,$(HOST_PSEP),$(LIBDIR)),$(HOST_MKDIR)); fi
	$(AR) $(LIB) $(OBJS)
	$(RANLIB) -static $(LIB) $(OBJS)
```

Add this to config.h file in pjmedia/include/pjmedia:

```
/** Constant for AudioQueue sound backend. */
#define PJMEDIA_SOUND_IPOD_SOUND            4
```

Copy a header that is missing in the device includes

```
cp /Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator2.1.sdk/usr/include/netinet/in_systm.h \
/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.1.sdk/usr/include/netinet/in_systm.h
```

Edit os-auto.mak.in pjmedia/build and add

```
#
# iPod/iPhone
#
ifeq ($(AC_PJMEDIA_SND),ipod)
export SOUND_OBJS = ipodsound.o
export CFLAGS += -DPJMEDIA_SOUND_IMPLEMENTATION=PJMEDIA_SOUND_IPOD_SOUND
endif
```

Comment out the media tests build target in pjmedia/build/makefile 109:

```
TARGETS := pjmedia pjmedia-codec pjsdp #pjmedia-test
```

Generate configuration script

```
autoconf aconfigure.ac > aconfigure
```

aconfigure with these options seem to work

```
./aconfigure --host=arm-apple-darwin9 --disable-ssl --disable-speex-aec \
--disable-speex-codec --disable-l16-codec --disable-g722-codec \
--disable-ilbc-codec CFLAGS="-arch armv6 -pipe -O0 -isysroot \
 /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.1.sdk \
-I/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.1.sdk/usr/include/gcc/darwin/4.0" \
LDFLAGS="-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.1.sdk \
-L/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/IphoneOS2.1.sdk/usr/lib" \
CPP=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-cpp \
AR=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-ar \
RANLIB=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-ranlib \
CC=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-gcc
```

now to setup link in xcode...