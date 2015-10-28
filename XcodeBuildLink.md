# Introduction #

Build pjproject first using these [instructions](http://code.google.com/p/voiphone/wiki/BuildPJsipSDK21)

This page attempts to describe how to link the pjsip libraries in your own xcode project for the iPhone SDK.

You will need to adjust the paths to the libraries to suit your directory structure.

# Details #

Set the following in the project properties for **All Configurations**. However, this probably won't work on the Simulator.

### Header Search Paths ###
`pjlib/include pjlib-util/include pjnath/include pjmedia/include pjsip/include third_party/include`

### Library Search Paths ###
`pjlib/lib pjlib-util/lib pjnath/lib pjmedia/lib pjsip/lib third_party/lib`

### Other C Flags ###
`-DPJ_DARWINOS`
### Other Linker Flags ###
`-lpjsua-arm-apple-darwin9 -lpjsip-ua-arm-apple-darwin9 -lpjsip-simple-arm-apple-darwin9 -lpjsip-arm-apple-darwin9 -lpjmedia-codec-arm-apple-darwin9 -lpjmedia-arm-apple-darwin9 -lpjnath-arm-apple-darwin9 -lpjlib-util-arm-apple-darwin9 -lpj-arm-apple-darwin9 -lm -lpthread`