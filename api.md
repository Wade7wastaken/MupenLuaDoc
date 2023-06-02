---

# emu.atinput

Registers a function to be run every input frame


```lua
function emu.atinput(func: function, unregister?: boolean)
```

---

# emu.atinterval

Registers a function to be run constantly


```lua
function emu.atinterval(func: function, unregister?: boolean)
```

---

# emu.atloadstate

Registers a function to be run when a savestate is loaded


```lua
function emu.atloadstate(func: function, unregister?: boolean)
```

---

# emu.atreset

Registers a function to be run when the emulator is reset


```lua
function emu.atreset(func: function, unregister?: boolean)
```

---

# emu.atsavestate

Registers a function to be run when a savestate is saved


```lua
function emu.atsavestate(func: function, unregister?: boolean)
```

---

# emu.atstop

Registers a function to be run when the lua script is stopped


```lua
function emu.atstop(func: function, unregister?: boolean)
```

---

# emu.atupdatescreen

Registers a function to be run when the screen updates


```lua
function emu.atupdatescreen(func: function, unregister?: boolean)
```

---

# emu.atvi

Registers a function to be run every VI


```lua
function emu.atvi(func: function, unregister?: boolean)
```

---

# emu.console

Similar to print, but only accepts strings


```lua
function emu.console(str: string)
```

---

# emu.framecount

Returns the number of VIs since the last movie was played. This should match the statusbar (assuming you have `0-index statusbar` off). If no movie has been played, it returns the number of VIs since the emulator was started, not reset


```lua
function emu.framecount()
  -> framecount: integer
```

---

# emu.getaddress

?


```lua
function emu.getaddress(address: string)
```

---

# emu.getpause

Returns `1` if the emulator is paused and `0` if it is not


```lua
function emu.getpause()
  -> emu_paused: boolean
```

---

# emu.getspeed

Returns the current speed limit of the emulator


```lua
function emu.getspeed()
  -> speed_limit: integer
```

---

# emu.getsystemmetrics

Gets a system metric using the windows [GetSystemMetrics](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics) function


```lua
function emu.getsystemmetrics(param: integer)
  -> metric: integer
```

---

# emu.getversion

Returns the current mupen version

@*param* `type` — `1` returns `Mupen 64 {version}`. `2` returns `{version}`

```lua
type:
    | 1
    | 2
```


```lua
function emu.getversion(type: 1|2)
  -> version: string
```

---

# emu.inputcount

Returns the number of input frames that have happened since the emulator was started. It does not reset when a movie is started


```lua
function emu.inputcount()
  -> inputcount: integer
```

---

# emu.isreadonly

Returns true if the currently playing movie is read only


```lua
function emu.isreadonly()
  -> read_only: boolean
```

---

# emu.pause

Pauses or unpauses the emulator

@*param* `pause` — True pauses the emulator and false resumes the emulator


```lua
function emu.pause(pause: boolean)
```

---

# emu.samplecount

Returns the number of input frames since the last movie was played. This should match the statusbar (assuming you have `0-index statusbar` off). If no movie is playing, it will return the last value when a movie was playing. If no movie has been played yet, it will return -1


```lua
function emu.samplecount()
  -> samplecount: integer
```

---

# emu.speed

Sets the speed limit of the emulator


```lua
function emu.speed(speed_limit: integer)
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
```

---

# emu.statusbar

Sets the statusbar text to `str`


```lua
function emu.statusbar(str: string)
```

---

# input.diff

Gets the difference in two input tables


```lua
function input.diff(t1: table, t2: table)
  -> table
```

---

# input.get

Returns a table of booleans corresponding to which keys are pressed


```lua
function input.get()
  -> table
```

---

# input.prompt

Unknown


```lua
function input.prompt()
```

---

# memory.LB

Loads a signed byte from rdram and returns it. Alias for `memory.readbytesigned`

@*param* `address` — The address to read from

@*return* `value` — The signed byte at `address`


```lua
function memory.LB(address: integer)
  -> value: integer
```

---

# memory.LBU

Loads an unsigned byte from rdram and returns it. Alias for `memory.readbyte`

@*param* `address` — The address to read from

@*return* `value` — The unsigned byte at `address`


```lua
function memory.LBU(address: integer)
  -> value: integer
```

---

# memory.LD

Loads a signed long long (8 bytes) from rdram and returns it in a table of 2 integers. Alias for `memory.readqwordsigned`

@*param* `address` — The address to read from

@*return* `value` — A table containing the the upper and lower 4 bytes of the signed long at `address`


```lua
function memory.LD(address: integer)
  -> value: table
```

---

# memory.LDC1

Loads a double (8 bytes) from rdram and returns it. Alias for `memory.readdouble`

@*param* `address` — The address to read from

@*return* `value` — The double at `address`


```lua
function memory.LDC1(address: integer)
  -> value: number
```

---

# memory.LDU

Loads a unsigned long long (8 bytes) from rdram and returns it in a table of 2 integers. Alias for `memory.readqword`

@*param* `address` — The address to read from

@*return* `value` — A table containing the the upper and lower 4 bytes of the unsigned long long at `address`


```lua
function memory.LDU(address: integer)
  -> value: table
```

---

# memory.LH

Loads a signed short (2 bytes) from rdram and returns it. Alias for `memory.readwordsigned`

@*param* `address` — The address to read from

@*return* `value` — The signed short at `address`


```lua
function memory.LH(address: integer)
  -> value: integer
```

---

# memory.LHU

Loads an unsigned short (2 bytes) from rdram and returns it. Alias for `memory.readword`

@*param* `address` — The address to read from

@*return* `value` — The unsigned short at `address`


```lua
function memory.LHU(address: integer)
  -> value: integer
```

---

# memory.LW

Loads a signed long (4 bytes) from rdram and returns it. Alias for `memory.readdwordsigned`

@*param* `address` — The address to read from

@*return* `value` — The signed long at `address`


```lua
function memory.LW(address: integer)
  -> value: integer
```

---

# memory.LWC1

Loads a float (4 bytes) from rdram and returns it. Alias for `memory.readfloat`

@*param* `address` — The address to read from

@*return* `value` — The float at `address`


```lua
function memory.LWC1(address: integer)
  -> value: number
```

---

# memory.LWU

Loads an unsigned long (4 bytes) from rdram and returns it. Alias for `memory.readdword`

@*param* `address` — The address to read from

@*return* `value` — The unsigned long at `address`


```lua
function memory.LWU(address: integer)
  -> value: integer
```

---

# memory.loadsize

Loads `size` bytes at `address` from rdram and returns it. Alias for `memory.readsize`

@*param* `address` — The address to read from

@*param* `size` — The size to read. Must be `1`, `2`, `4`, or `8`

@*return* `value` — `size` bytes at `address`


```lua
function memory.loadsize(address: integer, size: integer)
  -> value: number
```

---

# print

Prints a value to the lua console


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

# wgui.deleteimage

Clears one of all images

@*param* `idx` — The identifier of the image to clear. If it is 0, clear all iamges


```lua
function wgui.deleteimage(idx: integer)
```

---

# wgui.drawimage

Draws the image at index `idx` at the specified coordinates


```lua
function wgui.drawimage(idx: integer, x: integer, y: integer)
```

---

# wgui.drawimage

Draws the image at index `idx` at the specified coordinates and scale


```lua
function wgui.drawimage(idx: integer, x: integer, y: integer, s: number)
```

---

# wgui.drawimage

Draws the image at index `idx` at the specified coordinates and size


```lua
function wgui.drawimage(idx: integer, x: integer, y: integer, w: integer, h: integer)
```

---

# wgui.drawimage

Draws the image at index `idx` at the specified coordinates, size, and rotation, using a part of the source image given by the `src` parameters


```lua
function wgui.drawimage(idx: integer, x: integer, y: integer, w: integer, h: integer, srcx: integer, srcy: integer, srcw: integer, srch: integer, rotate: number)
```

---

# wgui.drawtext

GDI: Draws text in a rectangle (more documentation soon)


```lua
function wgui.drawtext(text: string, rect: table, format?: string)
```

---

# wgui.ellipse

Draws an ellipse at the specified coordinates and size. Uses the GDI brush color for the background and a 1 pixel border of the GDI pen color


```lua
function wgui.ellipse(left: integer, top: integer, right: integer, bottom: integer)
```

---

# wgui.fillellipsea

GDIPlus: Draws an ellipse at the specified coordinates, size, and color

@*param* `color` — Color names are currently broken

```lua
--  colors can be any of these or "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBA"
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
function wgui.fillellipsea(x: integer, y: integer, w: integer, h: integer, color: color)
```

---

# wgui.fillpolygona

Draws a filled in polygon using the points in `points`

@*param* `points` — Ex: `\{{x1, y1}, {x2, y2}, {x3, y3}\}`

@*param* `color` — Color names are currently broken

```lua
--  colors can be any of these or "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBA"
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
function wgui.fillpolygona(points: table, color: color)
```

---

# wgui.fillrect

Draws a rectangle at the specified coordinates with the specified color


```lua
function wgui.fillrect(left: integer, top: integer, right: integer, bottom: integer, red: integer, green: integer, blue: integer)
```

---

# wgui.fillrecta

GDIPlus: Draws a rectangle at the specified coordinates, size and color

@*param* `color` — Color names are currently broken

```lua
--  colors can be any of these or "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBA"
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
function wgui.fillrecta(x: integer, y: integer, w: integer, h: integer, color: color)
```

---

# wgui.getimageinfo

Returns the width and height of the image at `idx`


```lua
function wgui.getimageinfo(idx: integer)
  -> { width: integer, height: integer }
```

---

# wgui.info

Returns the current width and height of the mupen window in a table


```lua
function wgui.info()
  -> { width: integer, height: integer }
```

---

# wgui.line

Draws a line from `(x1, y1)` to `(x2, y2)`


```lua
function wgui.line(x1: integer, y1: integer, x2: integer, y2: integer)
```

---

# wgui.loadimage

Loads an image file from `path` and returns the identifier of that image


```lua
function wgui.loadimage(path: string)
  -> integer
```

---

# wgui.loadscreen

Captures the current screen and saves it as an image

@*return* `The` — identifier of the saved image


```lua
function wgui.loadscreen()
  -> The: integer
```

---

# wgui.loadscreenreset

Re-initializes loadscreen


```lua
function wgui.loadscreenreset()
```

---

# wgui.polygon

Draws a polygon with the given points. Uses the GDI brush color for the background and a 1 pixel border of the GDI pen color


```lua
function wgui.polygon(points: table)
```

---

# wgui.rect

GDI: Draws a rectangle at the specified coordinates with the current GDI background color and a 1 pixel border of the GDI pen color.

@*param* `rounded_width` — The width of the ellipse used to draw the rounded corners. (currently don't seem to be working)

@*param* `rounded_height` — The height of the ellipse used to draw the rounded corners.


```lua
function wgui.rect(left: integer, top: integer, right: integer, bottom: integer, rounded_width?: integer, rounded_height?: integer)
```

---

# wgui.resize

Resizes the mupen window to `w` x `h`


```lua
function wgui.resize(w: integer, h: integer)
```

---

# wgui.setbk

GDI: Sets the current GDI background color to `color`

```lua
--  colors can be any of these or "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBA"
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
function wgui.setbk(color: color)
```

---

# wgui.setbrush

GDI: Sets the current GDI brush color to `color`

```lua
--  colors can be any of these or "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBA"
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
function wgui.setbrush(color: color)
```

---

# wgui.setcolor

GDI: Sets the current GDI text color to `color`

```lua
--  colors can be any of these or "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBA"
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
```

---

# wgui.setfont

Sets the font, font size, and font style

@*param* `size` — The size of the font. Defaults to 0 if not given

@*param* `font` — The name of the font from the operating system. Dafaults to "MS Gothic" if not given

@*param* `style` — Each character in this string sets one style of the font, applied in chronological order. `b` sets bold, `i` sets italics, `u` sets underline, `s` sets strikethrough, and `a` sets antialiased. Defaults to "" if not given


```lua
function wgui.setfont(size?: integer, font?: string, style?: string)
```

---

# wgui.setpen

GDI: Sets the current GDI pen color to `color`

```lua
--  colors can be any of these or "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBA"
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
function wgui.setpen(color: color)
```

---

# wgui.text

GDI: Displays text in one line with the current GDI background color and GDI text color


```lua
function wgui.text(x: integer, y: integer, text: string)
```

