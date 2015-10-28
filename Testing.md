# Introduction #

This details how to test pjsua on your iPhone running FW 2.1

# Details #

  1. copy pjsip-apps/bin/pjsua-arm-apple-darwin9 to your iphone with scp
  1. ssh to the iphone and run ldid -S pjsua-arm-apple-darwin9
  1. create a iphone.cfg file with the following content
```
--app-log-level=1
--local-port=5063
--no-tcp

--dis-codec=iLBC

--clock-rate=8000
--snd-clock-rate=8000
--ec-tail=0
--quality=2
--no-vad 

--snd-auto-close=0

--add-buddy sip:sipuraspa:5061
```
  1. run ./pjsua-arm-apple-darwin9 --config-file=iphone.cfg from the command line and **M** ake some test calls