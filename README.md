APNSd
=====

Apple Push Notification Service daemon

### Building ###

Simply run qmake followed by make. (Requires Qt 4.8 or later)

### Configure ###

Run APNSd once to generate the settings file. (/etc/APNSd.cfg)

### Running ###

Run in foreground:
```
./APNSd
```

Run as daemon:
```
./APNSd d
```

### Usage ###

To send a push payload use:
```
./APNSd push <hexadecimal device token> <base64 encoded json payload>
```

### TODO ###

-Proper feedback service implementation.

### Note ###

Tested on Debian Wheezy.
