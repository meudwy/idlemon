# idlemon

*Idle Monitor* is an application that executes tasks based on the amount of
time the system has been idle.

## Usage

```
Usage: idlemon [options]

Execute tasks based on the time the system has been idle.

Options:
  -c <filename> (default: ~/.config/idlemon.conf) config filename
  -p            ping active instance
  -r            reload config of active instance

```

## Example Config

The following configuration will lock the screen when the screensaver activates,
and shut the system down after 24 hours.

```
[task]
name = Lock Screen
argv = slock
delay = xss

[task]
name = Shutdown
argv = systemctl poweroff
delay = 24h
```

## ScreenSaver

If delay is set to `xss` the task is only executed when the screensaver is
activated.

Most software capable of playing video (mpv, firefox, chromium, ...) will call
`XScreenSaverSuspend()` to prevent the screensaver from activating.

Use the `xset` command to adjust the screensaver timeout:

```sh
$ xset s 600 600
```

