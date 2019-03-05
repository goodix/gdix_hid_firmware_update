### How to do a Firmware update
Boot up your target device and login in.
In the browser,press Ctrl+Alt+T to open a terminal,type 'shell' enter the shell.

Copy this folder to the target device,put it in someplace,like /usr/local/.

To find the goodix hid device path,go to `/sys/bus/hid/device`,find a hid device that VID match `27C6`,

you should find some folder looks like this `0018:27C6:01F0.0001`.

Next,enter this folder, `'/sys/bus/hid/devices/0018:27C6:01F0.0001/hidraw'`,

type `ls` to list all folder,you should find a folder looks like `'hidrawx'`,this is the device path of the goodix hid device,

let's say hidraw0 for the next step.

Go back to /usr/local/,type the follow cmd,

    sudo gdixupdate -d /dev/hidraw0 -s 7388 -f -i <firmware>

`<firmware>` is the bin file that provided by goodix.

The output log will tell you whether the update is success.

