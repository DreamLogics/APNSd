APNSd
=====

Apple Push Notification Service daemon

#### Building ####
Simply run qmake followed by make. (Requires Qt 4.8 or later)

#### Configure ####
Run APNSd once to generate the settings file. (/etc/APNSd.cfg)

#### Running ####
Run in foreground:
```
./APNSd
```

Run as daemon:
```
./APNSd d
```

When running as a daemon check syslog for errors:
```
tail /var/log/syslog | grep APNSd
```
To stop the daemon send a sigterm signal.

#### Usage ####
To send a push payload use:
```
./APNSd push <hexadecimal device token> <base64 encoded json payload>
```

#### TODO ####
-Proper feedback service implementation.   
-Implement push protocol v2.   
-Proper certificate validation

#### Note ####
Tested on Debian Wheezy.
