---

# emu.atinput

Calls the function `f` every input frame. The function `f` receives
an argument that seems to always be `0`. If `unregister` is set
to true, the function `f` will no longer be called when this event
occurs, but it will error if you never registered the function.


```lua
function emu.atinput(f: fun(a?: integer):nil, unregister?: boolean)
  -> nil
```

---

# emu.atinterval

Calls the function `f` constantly, even when the emulator is paused
If `unregister` is set to true, the function `f` will no longer
be called when this event occurs, but it will error if you never
registered the function.


```lua
function emu.atinterval(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atloadstate

Calls the function `f` when a savestate is loaded. If `unregister`
is set to true, the function `f` will no longer be called when
this event occurs, but it will error if you never registered the
function


```lua
function emu.atloadstate(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atplaymovie

Calls the function `f` when a movie is played. If `unregister`
is set to true, the function `f` will no longer be called when
this event occurs, but it will error if you never registered the
function.


```lua
function emu.atplaymovie(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atreset

Calls the function `f` when the emulator is reset. If `unregister`
is set to true, the function `f` will no longer be called when
this event occurs, but it will error if you never registered the
function


```lua
function emu.atreset(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atsavestate

Calls the function `f` when a savestate is saved. If `unregister`
is set to true, the function `f` will no longer be called when
this event occurs, but it will error if you never registered the
function


```lua
function emu.atsavestate(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atstop

Calls the function `f` when the script is stopped. If `unregister` is
set to true, the function `f` will no longer be called when this event
occurs, but it will error if you never registered the function.


```lua
function emu.atstop(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atstopmovie

Calls the function `f` when a movie is stopped. If `unregister`
is set to true, the function `f` will no longer be called when
this event occurs, but it will error if you never registered the
function


```lua
function emu.atstopmovie(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atupdatescreen

Seems similar to `emu.atvi`, except that it is called after.
If `unregister` is set to true, the function `f` will no longer
be called when this event occurs, but it will error if you
never registered the function.


```lua
function emu.atupdatescreen(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atvi

Calls the function `f` every vi frame. For example,
in Super Mario 64, the function will be called twice
when you advance by one frame whereas it will be called
once in Ocarina of Time. If `unregister` is set to true,
the function `f` will no longer be called when this event occurs,
but it will error if you never registered the function.


```lua
function emu.atvi(f: fun():nil, unregister?: boolean)
  -> nil
```

---

# emu.atwindowmessage

Defines a handler function that is called when a window receives a
message. The message data is given to the function in 4 parameters.
If `unregister` is set to true, the function `f` will no longer
be called when this event occurs, but it will error if you never
registered the function.


```lua
function emu.atwindowmessage(f: fun(a: any, b: any, c: any, d: any):nil, unregister?: boolean)
  -> nil
```

---

# emu.console

Displays the text message in the console.
Similar to `print`, but only accepts strings

@*param* `message` — The string to print to the console


```lua
function emu.console(message: string)
  -> nil
```

---

# emu.debugview

Prints `message` to the debug console. If you are not debugging
mupen with Visual Studio, this function will do nothing

@*param* `message` — The string to print to the debug console


```lua
function emu.debugview(message: string)
  -> nil
```

---

# emu.framecount

Returns the number of VIs since the last movie was played.
This should match the statusbar (assuming you have `0-index
statusbar` off). If no movie has been played, it returns the
number of VIs since the emulator was started, not reset


```lua
function emu.framecount()
  -> framecount: integer
```

---

# emu.getaddress

?


```lua
function emu.getaddress(address: string)
  -> integer
```

---

# emu.getpause

Returns `true` if the emulator is paused and `false` if it is not


```lua
function emu.getpause()
  -> emu_paused: boolean
```

---

# emu.getspeed

Returns the current speed limit (not the current speed) of the
emulator


```lua
function emu.getspeed()
  -> speed_limit: integer
```

---

# emu.getsystemmetrics

Gets a system metric using the windows
[GetSystemMetrics](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics)
function


```lua
function emu.getsystemmetrics(param: integer)
  -> metric: integer
```

---

# emu.getversion

Returns the current mupen version. If `type` is 0 or less, it
will return the full version name (Mupen 64 0.0.0). If `type`
is 1 or more, it will return the version number (0.0.0)

```lua
type:
    | 0
    | 1
```


```lua
function emu.getversion(type: 0|1)
  -> version: string
```

---

# emu.inputcount

Returns the number of input frames that have happened since
the emulator was started. It does not reset when a movie is
started


```lua
function emu.inputcount()
  -> inputcount: integer
```

---

# emu.ismainwindowinforeground

Returns `true` if the main mupen window is focused and false if
it is not


```lua
function emu.ismainwindowinforeground()
  -> focused: boolean
```

---

# emu.isreadonly

Returnss true if the currently playing movie is read only and
false if it is not


```lua
function emu.isreadonly()
  -> read_only: boolean
```

---

# emu.pause

Pauses or unpauses the emulator

@*param* `pause` — True pauses the emulator and false resumes

it


```lua
function emu.pause(pause: boolean)
```

---

# emu.samplecount

Returns the number of input frames since the last movie was
played. This should match the statusbar (assuming you have
`0-index statusbar` off). If no movie is playing, it will return
the last value when a movie was playing. If no movie has been
played yet, it will return `-1`


```lua
function emu.samplecount()
  -> samplecount: integer
```

---

# emu.screenshot

Takes a screenshot and saves it to the directory `dir`

@*param* `dir` — The directory to save the screenshot to


```lua
function emu.screenshot(dir: string)
  -> nil
```

---

# emu.setgfx

?

```lua
mode:
    | 0
    | 1
```


```lua
function emu.setgfx(mode: 0|1)
  -> nil
```

---

# emu.speed

Sets the speed limit of the emulator


```lua
function emu.speed(speed_limit: integer)
  -> nil
```

---

# emu.speedmode

Sets the speed mode of the emulator

```lua
mode:
    | "normal"
    | "maximum"
```


```lua
function emu.speedmode(mode: "maximum"|"normal")
  -> nil
```

---

# emu.statusbar

Displays the text `message` in the status bar
on the bottom while replacing any other text.
The message will only display until the next frame.

@*param* `message` — The string to display on the status bar


```lua
function emu.statusbar(message: string)
  -> nil
```

---

# memory.LB

Loads a signed byte from rdram and returns it. Alias for
`memory.readbytesigned`

@*param* `address` — The address to read from

@*return* `value` — The signed byte at `address`


```lua
function memory.LB(address: integer)
  -> value: integer
```

---

# memory.LBU

Loads an unsigned byte from rdram and returns it. Alias for
`memory.readbyte`

@*param* `address` — The address to read from

@*return* `value` — The unsigned byte at `address`


```lua
function memory.LBU(address: integer)
  -> value: integer
```

---

# memory.LD

Loads a signed quad word (8 bytes) from rdram and returns it.
Alias for `memory.readqwordsigned`

@*param* `address` — The address to read from

@*return* `value` — A table containing the upper and lower

halves of the unsigned quad word at `address`


```lua
function memory.LD(address: integer)
  -> value: integer[]
```

---

# memory.LDC1

Loads a double (8 bytes) from rdram and returns it. Alias for
`memory.readdouble`

@*param* `address` — The address to read from

@*return* `value` — the double at `address`


```lua
function memory.LDC1(address: integer)
  -> value: number
```

---

# memory.LDU

Loads an unsigned quad word (8 bytes) from rdram and returns it.
Alias for `memory.readqword`

@*param* `address` — The address to read from

@*return* `value` — A table containing the upper and lower

halves of the unsigned quad word at `address`


```lua
function memory.LDU(address: integer)
  -> value: integer[]
```

---

# memory.LH

Loads a signed word (2 bytes) from rdram and returns it. Alias
for `memory.readwordsigned`

@*param* `address` — The address to read from

@*return* `value` — The signed word at `address`


```lua
function memory.LH(address: integer)
  -> value: integer
```

---

# memory.LHU

Loads an unsigned word (2 bytes) from rdram and returns it. Alias
for `memory.readword`

@*param* `address` — The address to read from

@*return* `value` — The unsigned word at `address`


```lua
function memory.LHU(address: integer)
  -> value: integer
```

---

# memory.LW

Loads a signed double word (4 bytes) from rdram and returns it.
Alias for `memory.readdwordsigned`

@*param* `address` — The address to read from

@*return* `value` — The signed double word at `address`


```lua
function memory.LW(address: integer)
  -> value: integer
```

---

# memory.LWC1

Loads a float (4 bytes) from rdram and returns it. Alias for
`memory.readfloat`

@*param* `address` — The address to read from

@*return* `value` — the float at `address`


```lua
function memory.LWC1(address: integer)
  -> value: number
```

---

# memory.LWU

Loads an unsigned double word (4 bytes) from rdram and returns it.
Alias for `memory.readdword`

@*param* `address` — The address to read from

@*return* `value` — The unsigned double word at `address`


```lua
function memory.LWU(address: integer)
  -> value: integer
```

---

# memory.loadbytes

Currently broken


```lua
function memory.loadbytes(address: integer, size: integer)
```

---

# memory.loadhalfs

Currently broken


```lua
function memory.loadhalfs(address: integer, size: integer)
```

---

# memory.loadsize

Reads 1, 2, 4, or 8 bytes from rdram and returns it. Alias for
`memory.readsize`

@*param* `address` — The address to read from

@*param* `size` — The number of bytes to read. Must be `1`,

`2`, `4`, or `8`

```lua
size:
    | 1
    | 2
    | 4
    | 8
```


```lua
function memory.loadsize(address: integer, size: 1|2|4|8)
  -> integer|integer[]
```

---

# memory.loadwords

Currently broken


```lua
function memory.loadwords(address: integer, size: integer)
```

---

# print

Prints a value to the lua console

@*param* `data` — The data to print to the console


```lua
function print(data: any)
```

---

# stop

Stops script execution


```lua
function stop()
```

---

# wgui.setbk

Sets the GDI background color to `color`

@*param* `color` — A color of "null" sets

the background color to transparent

```lua
-- colors can be any of these or "#RGB", "#RGBA",
-- "#RRGGBB", or "#RRGGBBA"
color:
    | "white"
    | "black"
    | "clear"
    | "gray"
    | "red"
    | "orange"
    | "yellow"
    | "chartreuse"
    | "green"
    | "teal"
    | "cyan"
    | "blue"
    | "purple"
    | "null"
```


```lua
function wgui.setbk(color: "null"|color)
  -> nil
```

---

# wgui.setbrush

Sets the GDI brush to `color`

@*param* `color` — A color of "null" resets

the brush to its default value

```lua
-- colors can be any of these or "#RGB", "#RGBA",
-- "#RRGGBB", or "#RRGGBBA"
color:
    | "white"
    | "black"
    | "clear"
    | "gray"
    | "red"
    | "orange"
    | "yellow"
    | "chartreuse"
    | "green"
    | "teal"
    | "cyan"
    | "blue"
    | "purple"
    | "null"
```


```lua
function wgui.setbrush(color: "null"|color)
  -> nil
```

---

# wgui.setcolor

Sets the GDI text color to `color`

```lua
-- colors can be any of these or "#RGB", "#RGBA",
-- "#RRGGBB", or "#RRGGBBA"
color:
    | "white"
    | "black"
    | "clear"
    | "gray"
    | "red"
    | "orange"
    | "yellow"
    | "chartreuse"
    | "green"
    | "teal"
    | "cyan"
    | "blue"
    | "purple"
```


```lua
function wgui.setcolor(color: color)
  -> nil
```

---

# wgui.setfont

Sets the GDI font to `size`, `font`, and `style`

@*param* `size` — The size is stored as an integer, but some opperations

are done before that so only integers should not be allowed

@*param* `font` — Defaults to "MS Gothic"

@*param* `style` — Defaults to "". Each character is processed to change

the style. `b` sets bold, `i` sets italics, `u` sets underline, `s` sets
strikethrough, and `a` sets antialiasing


```lua
function wgui.setfont(size: number, font?: string, style?: string)
  -> nil
```

---

# wgui.setpen

Sets the GDI pen color to `color` and `width`

@*param* `color` — A color of "null" resets

the pen to its default value

@*param* `width` — Defaults to 1

```lua
-- colors can be any of these or "#RGB", "#RGBA",
-- "#RRGGBB", or "#RRGGBBA"
color:
    | "white"
    | "black"
    | "clear"
    | "gray"
    | "red"
    | "orange"
    | "yellow"
    | "chartreuse"
    | "green"
    | "teal"
    | "cyan"
    | "blue"
    | "purple"
    | "null"
```


```lua
function wgui.setpen(color: "null"|color, width?: integer)
  -> nil
```

---

# wgui.text

Draws the text `text` at the specified coordinates. Uses the GDI font, 
the GDI background, and the GDI text color


```lua
function wgui.text(x: integer, y: integer, text: string)
```

