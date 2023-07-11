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

The following configuration will lock the screen after 10 minutes of idle time,
and shut the system down after 24 hours.

```
[task]
name = Lock Screen
argv = slock
delay = 10m

[task]
name = Shutdown
argv = systemctl poweroff
delay = 24h
```

