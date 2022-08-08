# sgestures

# Install
```
make
make install
```

# Usage
```
sgestures-libinput-writer | sgestures
```

This will use configuration in `${XDG_CONFIG_HOME:-$HOME/.config}/sgestures/`.

After modifying, sgestures can be updated with `sgestures --recompile`

## System-wide
See [mqbus](https://codeberg.org/TAAPArthur/mqbus) on how the above pipeline
can be modified when the writer is used as a system service.

## Alternative backend
You don't have to use libinput. sgestures-libinput-writer is just a translation
layer around libinput into our internal format. See [this wip
repo](https://codeberg.org/TAAPArthur/inputhandler) for a linux-specific
alternative

# Troubleshooting
`make debug`

By default it will listen and print received events
