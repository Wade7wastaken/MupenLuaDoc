---@meta

-- This file has meta definitions for the functions implemented in mupen64.
-- https://github.com/mkdasher/mupen64-rr-lua-/blob/master/lua/LuaConsole.cpp

-- Additional documentation can be found here:
-- https://docs.google.com/document/d/1SWd-oAFBKsGmwUs0qGiOrk3zfX9wYHhi3x5aKPQS_o0

emu = {}
memory = {}
gui = {}
wgui = {}
input = {}
joypad = {}
savestate = {}
iohelper = {}


-- Global Functions
--#region

---Prints a value to the lua console
---@param data any
function print(data)
end

---Stops script execution
function stop()
end

--#endregion

-- emu Functions
--#region

---Similar to print, but only accepts strings
---@deprecated Use `print`
---@param str string
function emu.console(str)
end

---Sets the statusbar text to `str`
---@param str string
function emu.statusbar(str)
end

---Registers a function to be run every VI
---@param func function
---@param unregister boolean?
function emu.atvi(func, unregister)
end

---Registers a function to be run when the screen updates
---@param func function
---@param unregister boolean?
function emu.atupdatescreen(func, unregister)
end

---Registers a function to be run every input frame
---@param func function
---@param unregister boolean?
function emu.atinput(func, unregister)
end

---Registers a function to be run when the lua script is stopped
---@param func function
---@param unregister boolean?
function emu.atstop(func, unregister)
end

---Registers a function to be run constantly
---@param func function
---@param unregister boolean?
function emu.atinterval(func, unregister)
end

---Registers a function to be run when a savestate is loaded
---@param func function
---@param unregister boolean?
function emu.atloadstate(func, unregister)
end

---Registers a function to be run when a savestate is saved
---@param func function
---@param unregister boolean?
function emu.atsavestate(func, unregister)
end

---Registers a function to be run when the emulator is reset
---@param func function
---@param unregister boolean?
function emu.atreset(func, unregister)
end

---Returns the number of VIs since the last movie was played. This should match the statusbar (assuming you have `0-index statusbar` off). If no movie has been played, it returns the number of VIs since the emulator was started, not reset
---@return integer framecount
function emu.framecount()
end

---Returns the number of input frames since the last movie was played. This should match the statusbar (assuming you have `0-index statusbar` off). If no movie is playing, it will return the last value when a movie was playing. If no movie has been played yet, it will return -1
---@return integer samplecount
function emu.samplecount()
end

---Returns the number of input frames that have happened since the emulator was started. It does not reset when a movie is started
---@return integer inputcount
function emu.inputcount()
end

---Returns the current mupen version
---@param type 1|2 `1` returns `Mupen 64 {version}`. `2` returns `{version}`
---@return string version
function emu.getversion(type)
end

---Pauses or unpauses the emulator
---@param pause boolean True pauses the emulator and false resumes the emulator
function emu.pause(pause)
end

---Returns `1` if the emulator is paused and `0` if it is not
---@return boolean emu_paused
function emu.getpause()
end

---Returns the current speed limit of the emulator
---@return integer speed_limit
function emu.getspeed()
end

---Sets the speed limit of the emulator
---@param speed_limit integer
function emu.speed(speed_limit)
end

---Sets the speed mode of the emulator
---@param mode "normal"|"maximum"
function emu.speedmode(mode)
end

---?
---@param address string
function emu.getaddress(address)
end

---Returns true if the currently playing movie is read only
---@return boolean read_only
function emu.isreadonly()
end

---Gets a system metric using the windows [GetSystemMetrics](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics) function
---@param param integer
---@return integer metric
function emu.getsystemmetrics(param)
end

--#endregion

-- memory functions
--#region

---Loads an unsigned byte from rdram and returns it. Alias for `memory.readbyte`
---@param address integer The address to read from
---@return integer value The unsigned byte at `address`
function memory.LBU(address)
end

---Loads a signed byte from rdram and returns it. Alias for `memory.readbytesigned`
---@param address integer The address to read from
---@return integer value The signed byte at `address`
function memory.LB(address)
end

---Loads an unsigned short (2 bytes) from rdram and returns it. Alias for `memory.readword`
---@param address integer The address to read from
---@return integer value The unsigned short at `address`
function memory.LHU(address)
end

---Loads a signed short (2 bytes) from rdram and returns it. Alias for `memory.readwordsigned`
---@param address integer The address to read from
---@return integer value The signed short at `address`
function memory.LH(address)
end

---Loads an unsigned long (4 bytes) from rdram and returns it. Alias for `memory.readdword`
---@param address integer The address to read from
---@return integer value The unsigned long at `address`
function memory.LWU(address)
end

---Loads a signed long (4 bytes) from rdram and returns it. Alias for `memory.readdwordsigned`
---@param address integer The address to read from
---@return integer value The signed long at `address`
function memory.LW(address)
end

---Loads a unsigned long long (8 bytes) from rdram and returns it in a table of 2 integers. Alias for `memory.readqword`
---@param address integer The address to read from
---@return table value A table containing the the upper and lower 4 bytes of the unsigned long long at `address`
function memory.LDU(address)
end

---Loads a signed long long (8 bytes) from rdram and returns it in a table of 2 integers. Alias for `memory.readqwordsigned`
---@param address integer The address to read from
---@return table value A table containing the the upper and lower 4 bytes of the signed long at `address`
function memory.LD(address)
end

---Loads a float (4 bytes) from rdram and returns it. Alias for `memory.readfloat`
---@param address integer The address to read from
---@return number value The float at `address`
function memory.LWC1(address)
end

---Loads a double (8 bytes) from rdram and returns it. Alias for `memory.readdouble`
---@param address integer The address to read from
---@return number value The double at `address`
function memory.LDC1(address)
end

---Loads `size` bytes at `address` from rdram and returns it. Alias for `memory.readsize`
---@param address integer The address to read from
---@param size integer The size to read. Must be `1`, `2`, `4`, or `8`
---@return number value `size` bytes at `address`
function memory.loadsize(address, size)
end

--#endregion

--wgui Functions
--#region

-- colors can be any of these or "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBA"
---@alias color
---| string
---| "white"
---| "black"
---| "clear"
---| "gray"
---| "red"
---| "orange"
---| "yellow"
---| "chartreuse"
---| "green"
---| "teal"
---| "cyan"
---| "blue"
---| "purple"

---GDI: Sets the current GDI brush color to `color`
---@param color color
function wgui.setbrush(color)
end

---GDI: Sets the current GDI pen color to `color`
---@param color color
function wgui.setpen(color)
end

---GDI: Sets the current GDI text color to `color`
---@param color color
function wgui.setcolor(color)
end

---GDI: Sets the current GDI background color to `color`
---@param color color
function wgui.setbk(color)
end

---Sets the font, font size, and font style
---@param size integer? The size of the font. Defaults to 0 if not given
---@param font string? The name of the font from the operating system. Dafaults to "MS Gothic" if not given
---@param style string? Each character in this string sets one style of the font, applied in chronological order. `b` sets bold, `i` sets italics, `u` sets underline, `s` sets strikethrough, and `a` sets antialiased. Defaults to "" if not given
function wgui.setfont(size, font, style)
end

---GDI: Displays text in one line with the current GDI background color and GDI text color
---@deprecated Use `wgui.drawtext` instead
---@param x integer
---@param y integer
---@param text string
function wgui.text(x, y, text)
end

---GDI: Draws text in a rectangle (more documentation soon)
---@param text string
---@param rect table
---@param format string?
function wgui.drawtext(text, rect, format)
end

---GDI: Draws a rectangle at the specified coordinates with the current GDI background color and a 1 pixel border of the GDI pen color.
---@deprecated Use `wgui.fillrect` or `wgui.fillrecta` instead
---@param left integer
---@param top integer
---@param right integer
---@param bottom integer
---@param rounded_width integer? The width of the ellipse used to draw the rounded corners. (currently don't seem to be working)
---@param rounded_height integer? The height of the ellipse used to draw the rounded corners.
function wgui.rect(left, top, right, bottom, rounded_width, rounded_height)
end

---Draws a rectangle at the specified coordinates with the specified color
---@param left integer
---@param top integer
---@param right integer
---@param bottom integer
---@param red integer
---@param green integer
---@param blue integer
function wgui.fillrect(left, top, right, bottom, red, green, blue)
end

---GDIPlus: Draws a rectangle at the specified coordinates, size and color
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param color color|string Color names are currently broken
function wgui.fillrecta(x, y, w, h, color)
end

---GDIPlus: Draws an ellipse at the specified coordinates, size, and color
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param color color|string Color names are currently broken
function wgui.fillellipsea(x, y, w, h, color)
end

---Draws a filled in polygon using the points in `points`
---@param points table Ex: `\{\{x1, y1\}, \{x2, y2\}, \{x3, y3\}\}`
---@param color color|string Color names are currently broken
function wgui.fillpolygona(points, color)
end

---Loads an image file from `path` and returns the identifier of that image
---@param path string
---@return integer
function wgui.loadimage(path)
end

---Clears one of all images
---@param idx integer The identifier of the image to clear. If it is 0, clear all iamges
function wgui.deleteimage(idx)
end

---Draws the image at index `idx` at the specified coordinates
---@param idx integer
---@param x integer
---@param y integer
function wgui.drawimage(idx, x, y)
end

---Draws the image at index `idx` at the specified coordinates and scale
---@param idx integer
---@param x integer
---@param y integer
---@param s number
function wgui.drawimage(idx, x, y, s)
end

---Draws the image at index `idx` at the specified coordinates and size
---@param idx integer
---@param x integer
---@param y integer
---@param w integer
---@param h integer
function wgui.drawimage(idx, x, y, w, h)
end

---Draws the image at index `idx` at the specified coordinates, size, and rotation, using a part of the source image given by the `src` parameters
---@param idx integer
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param srcx integer
---@param srcy integer
---@param srcw integer
---@param srch integer
---@param rotate number
function wgui.drawimage(idx, x, y, w, h, srcx, srcy, srcw, srch, rotate)
end

---Captures the current screen and saves it as an image
---@return integer The identifier of the saved image
function wgui.loadscreen()
end

---Re-initializes loadscreen
function wgui.loadscreenreset()
end

---Returns the width and height of the image at `idx`
---@param idx integer
---@return {width: integer, height: integer}
function wgui.getimageinfo(idx)
end

---Draws an ellipse at the specified coordinates and size. Uses the GDI brush color for the background and a 1 pixel border of the GDI pen color
---@param left integer
---@param top integer
---@param right integer
---@param bottom integer
function wgui.ellipse(left, top, right, bottom)
end

---Draws a polygon with the given points. Uses the GDI brush color for the background and a 1 pixel border of the GDI pen color
---@param points table
function wgui.polygon(points)
end

---Draws a line from `(x1, y1)` to `(x2, y2)`
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
function wgui.line(x1, y1, x2, y2)
end

---Returns the current width and height of the mupen window in a table
---@return {width: integer, height: integer}
function wgui.info()
end

---Resizes the mupen window to `w` x `h`
---@param w integer
---@param h integer
function wgui.resize(w, h)
end

--#endregion

--input Functions
--#region

---Returns a table of booleans corresponding to which keys are pressed
---@return table
function input.get()
end

---Gets the difference in two input tables
---@param t1 table
---@param t2 table
---@return table
function input.diff(t1, t2)
end

---Unknown
function input.prompt()
end

--#endregion

--joypad Functions
--#region
