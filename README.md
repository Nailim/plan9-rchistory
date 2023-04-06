# plan9-rchistory

## about

An attempt to bring command history through keyboard shortcuts to rc shell inside rio window, following the philosophy of modularity.

## why

Plan9 was built with a graphical interface in mind, which allows for some wonderful functionality (such as inline editing and execution of previous commands), and should be highly encouraged to try out.

That being said, there are use cases in which a command history functionality and keyboard interface would be useful: repetitive commands (executing same/similar command for testing or scanning, ...), faster recollection of already used commands (between context switches, extended usage sessions, crowded printouts, ...), access to previous commands between terminals and sessions (continuation of used commands trough time between sessions, reboots, ...), ...

But the most, in my opinion, important reason: **learning**! The frustration of dealing with the unknown operation of the system (mistyped command, experimenting with flags, pressure of remembering large quantities of information at once, ...) instead of learning the system, can (and will) impede the speed at which the knowledge is acquired or (in the worst case) deter from actually pursuing the acquisition of this particular knowledge. In short, its more fun to learn the thing, than to deal with the peripheral things needed to learn the thing.

## installation

### build

`mk`

### install

Installs to user folder $home/bin/rc and $home/bin/$objtype.

`mk install`

### uninstall

Removes from user folder.

`mk uninstall`

### configuration

For interactive mode, it should be configured as riow or reform/shortcuts component that uses /dev/kbdtap.

For example, the following line should be inserted in *$home/bin/rc/riostart* file:

`</dev/kbdtap histw >/dev/kbdtap`

If you're having issues with additional windows on startup, you can put the previous definition on to a separate script and call that from riostart file.

For example, create a script srw as *$home/bin/rc/srw* (with riow for pipe examples):

```
#!/bin/rc

# srw
# start riow bind

# set up riow with bar and command history
</dev/kbdtap hist -g | riow >/dev/kbdtap |[3] bar -p tl -d 'WW hh:mm:ss'
```

And then call the script in *$home/bin/rc/riostart* file:

`window srw`

By default, interactive mode searches through history from the current window (local history). Configuring interactive mode to include searching trough history saved from all terminal windows (global history) use "-g" flag in start up script. To search only trough global history, use "-G" flag.

## usage

### standalone

**hist**

Run *hist* command to print out the commands used in this terminal window. By default, it parses and prints history from the current terminal window (local history). To include history saved from all terminal windows (global history) use "-g" flag. To print out only global history, use "-G" flag.

If grepping trough hist output is too much of a hassle, add a wrapper function to your profile:

`fn h { if(test $#* '=' 0) hist -G; if not hist -G | grep $* }`

**savehist**

Run *savehist* command to parse and save current local history to global history, located in $home/lib/rchistory file.

Define a *quit* function to save and exit the current terminal window by defining the following rc function in your user profile:

`fn quit { savehist; exit }`

### interactive

**histw**

Use CTRL + UP or CTRL + DOWN key combinations to insert previous or next line of history in to the prompt.

To search trough history, type text to an empty prompt in terminal window before using key combinations. 

Pressing enter key (and running the selected command) or pressing delete key (canceling selected command) will reset interactive history to initial state.

## faq

### why why

Simple, let's learn something. This Plan9 thing is new to me too.

### what is plan9

Good question! I like you, you're trying to figure out something new. Unfortunately, outside the scope of this document. Please use the vast collection of knowledge of the internet to find out (even ChatGPT will be able to give you quite the explanation). To add to the confusion, this was developed on 9front.

### is this for me

In short, probably not. This requires some previous knowledge, and by the time that knowledge is acquired, you'll already have an answer to this question.

### could this be configured (or developed) better

Most probably. Please tell me how to improve it.

### current issues

Most probably. Please find some and let me know.
