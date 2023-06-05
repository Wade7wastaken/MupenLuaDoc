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
movie = {}
savestate = {}
ioHelper = {}
avi = {}

---@alias qword integer[] A representation of an 8 byte integer (quad word) as
---two 4 byte integers. This is sometime misnamed in the code as a dword

-- Global Functions
--#region

---Prints a value to the lua console
---@param data any The data to print to the console
function print(data) end

---Stops script execution
function stop() end

---Takes in a 4 byte integer and reinterprets the bits as a float. This does not
---convert from an integer to a float, but reinterprets the memory. This
---function should only be used as a last option. If you need to read a
---floating point number from memory, use `memory.readfloat` or `memory.LWC1`.
---@param data integer
---@return number
function MTC1(data) end

---Takes in an 8 byte integer as a table of two 4 byte integers and reinterprets
---the bits as a double. This does not convert from an integer to a double, but
---reinterprets the memory. This function should only be used as a last option.
---If you need to read a double floating point number from memory, use
---`memory.readdouble` or `memory.LDC1`.
---@param data qword
---@return number
function DMTC1(data) end

---Takes in a 4 byte single float and reinterprets the bits as a 4 byte integer.
---This does not convert from a float to an integer, but reinterprets the
---memory. This function should only be used as a last option.
---@param data number
---@return integer
function MFC1(data) end

---Takes in an 8 byte double float and reinterprets the bits as an 8 byte
---integer and returns it as a table of two 4 byte integers. This does not
---convert from a double float to an integer, but reinterprets the memory. This
---function should only be used as a last option.
---@param data number
---@return qword
function DMFC1(data) end

---Takes in an 8 byte integer as a table of two 4 byte integers and returns it
---as a number.
---@param data qword
---@return number
function CVT_D_L(data) end

--#endregion


-- emu functions
--#region

---Displays the text `message` in the console. Similar to `print`, but only
---accepts strings. Because of this, `print` should be used instead.
---@deprecated Use `print` instead.
---@param message string The string to print to the console.
---@return nil
function emu.console(message) end

---Prints `message` to the debug console. If you are not debugging mupen with
---Visual Studio, this function will do nothing.
---@deprecated This function has no use to the end user.
---@param message string The string to print to the debug console.
---@return nil
function emu.debugview(message) end

---Displays the text `message` in the status bar on the bottom while replacing
---any other text. The message will only display until the next frame.
---@param message string The string to display on the status bar.
---@return nil
function emu.statusbar(message) end

---Calls the function `f` every VI frame. For example, in Super Mario 64, the
---function will be called twice when you advance by one frame, whereas it will
---be called once in Ocarina of Time. If `unregister` is set to true, the
---function `f` will no longer be called when this event occurs, but it will
---error if you never registered the function.
---@param f fun(): nil The function to be called every VI frame.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atvi(f, unregister) end

---Similar to `emu.atvi`, except that it is called after. If `unregister` is set
---to true, the function `f` will no longer be called when this event occurs,
---but it will error if you never registered the function.
---@param f fun(): nil The function to be called after every VI frame.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atupdatescreen(f, unregister) end

---Calls the function `f` every input frame. The function `f` receives an
---argument that seems to always be `0`. If `unregister` is set to true, the
---function `f` will no longer be called when this event occurs, but it will
---error if you never registered the function. Alias for `joypad.register`.
---@param f fun(a: integer?): nil The function to be called every input frame. It receives an argument that seems to always be `0`.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atinput(f, unregister) end

---Calls the function `f` when the script is stopped. If `unregister` is set to
---true, the function `f` will no longer be called when this event occurs, but
---it will error if you never registered the function.
---@param f fun(): nil The function to be called when the script is stopped
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atstop(f, unregister) end

---Defines a handler function that is called when a window receives a message.
---The message data is given to the function in 4 parameters. If `unregister`
---is set to true, the function `f` will no longer be called when this event
---occurs, but it will error if you never registered the function.
---@param f fun(a, b, c, d): nil The function to be called when a window message is received.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atwindowmessage(f, unregister) end

---Calls the function `f` constantly, even when the emulator is paused. If
---`unregister` is set to true, the function `f` will no longer be called when
---this event occurs, but it will error if you never registered the function.
---@param f fun(): nil The function to be called constantly.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atinterval(f, unregister) end

---Calls the function `f` when a movie is played. If `unregister` is set to
---true, the function `f` will no longer be called when this event occurs, but
---it will error if you never registered the function.
---@param f fun(): nil The function to be called when a movie is played.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atplaymovie(f, unregister) end

---Calls the function `f` when a movie is stopped. If `unregister` is set to
---true, the function `f` will no longer be called when this event occurs, but
---it will error if you never registered the function.
---@param f fun(): nil The function to be called when a movie is stopped.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atstopmovie(f, unregister) end

---Calls the function `f` when a savestate is loaded. If `unregister` is set to
---true, the function `f` will no longer be called when this event occurs, but
---it will error if you never registered the function.
---@param f fun(): nil The function to be called when a savestate is loaded.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atloadstate(f, unregister) end

---Calls the function `f` when a savestate is saved. If `unregister` is set to
---true, the function `f` will no longer be called when this event occurs, but
---it will error if you never registered the function.
---@param f fun(): nil The function to be called when a savestate is saved.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atsavestate(f, unregister) end

---Calls the function `f` when the emulator is reset. If `unregister` is set to
---true, the function `f` will no longer be called when this event occurs, but
---it will error if you never registered the function.
---@param f fun(): nil The function to be called when the emulator is reset.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function emu.atreset(f, unregister) end

---Returns the number of VIs since the last movie was played. This should match
---the statusbar (assuming you have `0-index statusbar` off). If no movie has
---been played, it returns the number of VIs since the emulator was started, not
---reset.
---@nodiscard
---@return integer framecount The number of VIs since the last movie was played.
function emu.framecount() end

---Returns the number of input frames since the last movie was played. This
---should match the statusbar (assuming you have `0-index statusbar` off). If no
---movie is playing, it will return the last value when a movie was playing. If
---no movie has been played yet, it will return `-1`.
---@nodiscard
---@return integer samplecount The number of input frames since the last movie was played.
function emu.samplecount() end

---Returns the number of input frames that have happened since the emulator was
---started. It does not reset when a movie is started. Alias for `joypad.count`.
---@nodiscard
---@return integer inputcount The number of input frames that have happened since the emulator was started.
function emu.inputcount() end

---Returns the current mupen version. If `type` is 0 or less, it
---will return the full version name (Mupen 64 0.0.0). If `type`
---is 1 or more, it will return the version number (0.0.0).
---@nodiscard
---@param type 0|1 Whether to get the full version (less that 0) or the short version (more than 1).
---@return string version The mupen version
function emu.getversion(type) end

---Pauses or unpauses the emulator.
---@param pause boolean True pauses the emulator and false resumes it.
function emu.pause(pause) end

---Returns `true` if the emulator is paused and `false` if it is not.
---@nodiscard
---@return boolean emu_paused `true` if the emulator is paused and `false` if it is not.
function emu.getpause() end

---Returns the current speed limit (not the current speed) of the emulator.
---@nodiscard
---@return integer speed_limit The current speed limit of the emulator.
function emu.getspeed() end

---Sets the speed limit of the emulator.
---@param speed_limit integer The new speed limit of the emulator.
---@return nil
function emu.speed(speed_limit) end

---Sets the speed mode of the emulator.
---@param mode "normal"|"maximum"
---@return nil
function emu.speedmode(mode) end

---?
---@param mode 0|1
---@return nil
function emu.setgfx(mode) end

---?
---@nodiscard
---@param address string
---@return integer
function emu.getaddress(address) end

---Returns true if the currently playing movie is read only and false if it is
---not.
---@nodiscard
---@return boolean read_only
function emu.isreadonly() end

---Gets a system metric using the windows
---[GetSystemMetrics](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics)
---function.
---@nodiscard
---@param param integer
---@return integer metric
function emu.getsystemmetrics(param) end

---Returns `true` if the main mupen window is focused and false if it is not.
---@nodiscard
---@return boolean focused
function emu.ismainwindowinforeground() end

---Takes a screenshot and saves it to the directory `dir`
---@param dir string The directory to save the screenshot to
---@return nil
function emu.screenshot(dir) end

--#endregion


-- memory functions
--#region

---Loads an unsigned byte from rdram and returns it. Alias for
---`memory.readbyte`
---@nodiscard
---@param address integer The address to read from
---@return integer value The unsigned byte at `address`
function memory.LBU(address) end

---Loads a signed byte from rdram and returns it. Alias for
---`memory.readbytesigned`
---@nodiscard
---@param address integer The address to read from
---@return integer value The signed byte at `address`
function memory.LB(address) end

---Loads an unsigned word (2 bytes) from rdram and returns it. Alias
---for `memory.readword`
---@nodiscard
---@param address integer The address to read from
---@return integer value The unsigned word at `address`
function memory.LHU(address) end

---Loads a signed word (2 bytes) from rdram and returns it. Alias
---for `memory.readwordsigned`
---@nodiscard
---@param address integer The address to read from
---@return integer value The signed word at `address`
function memory.LH(address) end

---Loads an unsigned double word (4 bytes) from rdram and returns it.
---Alias for `memory.readdword`
---@nodiscard
---@param address integer The address to read from
---@return integer value The unsigned double word at `address`
function memory.LWU(address) end

---Loads a signed double word (4 bytes) from rdram and returns it.
---Alias for `memory.readdwordsigned`
---@nodiscard
---@param address integer The address to read from
---@return integer value The signed double word at `address`
function memory.LW(address) end

---Loads an unsigned quad word (8 bytes) from rdram and returns it.
---Alias for `memory.readqword`
---@nodiscard
---@param address integer The address to read from
---@return integer[] value A table containing the upper and lower halves of the unsigned quad word at `address`
function memory.LDU(address) end

---Loads a signed quad word (8 bytes) from rdram and returns it.
---Alias for `memory.readqwordsigned`
---@nodiscard
---@param address integer The address to read from
---@return integer[] value A table containing the upper and lower halves of the unsigned quad word at `address`
function memory.LD(address) end

---Loads a float (4 bytes) from rdram and returns it. Alias for
---`memory.readfloat`
---@nodiscard
---@param address integer The address to read from
---@return number value the float at `address`
function memory.LWC1(address) end

---Loads a double (8 bytes) from rdram and returns it. Alias for
---`memory.readdouble`
---@nodiscard
---@param address integer The address to read from
---@return number value the double at `address`
function memory.LDC1(address) end

---Reads 1, 2, 4, or 8 bytes from rdram and returns it. Alias for
---`memory.readsize`
---@param address integer The address to read from
---@param size 1|2|4|8 The number of bytes to read. Must be `1`, `2`, `4`, or `8`
---@return integer|integer[]
function memory.loadsize(address, size) end

---Currently broken
---@deprecated
---@nodiscard
---@param address integer
---@param size integer
function memory.loadbytes(address, size) end

---Currently broken
---@deprecated
---@nodiscard
---@param address integer
---@param size integer
function memory.loadhalfs(address, size) end

---Currently broken
---@deprecated
---@nodiscard
---@param address integer
---@param size integer
function memory.loadwords(address, size) end

-- function memory.SB() end

-- function memory.SH() end

-- function memory.SW() end

-- function memory.SD() end

-- function memory.SWC1() end

-- function memory.SDC1() end

-- function memory.storesize() end

-- function memory.syncbreak() end

-- function memory.pcbreak() end

-- function memory.readbreak() end

-- function memory.writebreak() end

-- function memory.reg() end

-- function memory.getreg() end

-- function memory.setreg() end

-- function memory.trace() end

-- function memory.tracemode() end

-- function memory.getcore() end

-- function memory.recompilenow() end

-- function memory.recompile() end

-- function memory.recompilenext() end

-- function memory.recompilenextall() end

-- function memory.readmemb() end

-- function memory.readmemh() end

-- function memory.readmem() end

-- function memory.readmemd() end

-- function memory.writememb() end

-- function memory.writememh() end

-- function memory.writemem() end

-- function memory.writememd() end

-- function memory.readbytesigned() end

-- function memory.readbyte() end

-- function memory.readwordsigned() end

-- function memory.readword() end

-- function memory.readdwordsigned() end

-- function memory.readdword() end

-- function memory.readqwordsigned() end

-- function memory.readqword() end

-- function memory.readfloat() end

-- function memory.readdouble() end

-- function memory.readsize() end

-- function memory.readbyterange() end

-- function memory.writebyte() end

-- function memory.writeword() end

-- function memory.writedword() end

-- function memory.writelong() end

-- function memory.writeqword() end

-- function memory.writefloat() end

-- function memory.writedouble() end

-- function memory.writesize() end

-- function memory.registerread() end

-- function memory.registerwrite() end

-- function memory.registerexec() end

-- function memory.getregister() end

-- function memory.setregister() end

--#endregion


-- gui functions
--#region

-- function gui.register() end

--#endregion


-- wgui functions
--#region

---colors can be any of these or "#RGB", "#RGBA",
---"#RRGGBB", or "#RRGGBBA"
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

---@alias align_format
---| string
---| "l" aligns the text to the left (already applied by default) (sets DT_LEFT)
---| "r" aligns the text to the right (sets DT_RIGHT)
---| "t" aligns the text to the top (already applied by default) (sets DT_TOP)
---| "b" aligns the text to the bottom (sets DT_BOTTOM)
---| "c" horizontally aligns the text to the center (sets DT_CENTER)
---| "v" vertically aligns the text to the center (sets DT_VCENTER)
---| "e" adds ellipsis if the text will not fit (sets DT_WORD_ELLIPSIS)
---| "s" forces the text to fit into a single line (sets DT_SINGLELINE)
---| "n" disables word break (unsets DT_WORDBREAK)
---more information [here](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-drawtext)


---Sets the GDI brush to `color`
---@param color color|"null" A color of "null" resets the brush to its default value
---@return nil
function wgui.setbrush(color) end

---Sets the GDI pen color to `color` and `width`
---@param color color|"null" A color of "null" resets the pen to its default value
---@param width integer? Defaults to 1
---@return nil
function wgui.setpen(color, width) end

---Sets the GDI text color to `color`
---@param color color
---@return nil
function wgui.setcolor(color) end

---Sets the GDI background color to `color`
---@param color color|"null" A color of "null" sets the background color to transparent
---@return nil
function wgui.setbk(color) end

---Sets the GDI font to `size`, `font`, and `style`
---@param size number The size is stored as an integer, but some operations are done before that so only integers should not be allowed
---@param font string? Defaults to "MS Gothic"
---@param style string? Defaults to "". Each character is processed to change the style. `b` sets bold, `i` sets italics, `u` sets underline, `s` sets strikethrough, and `a` sets antialiasing
---@return nil
function wgui.setfont(size, font, style) end

---Draws the text `text` at the specified coordinates. Uses the GDI font,
---GDI background, and GDI text color
---@deprecated use `wgui.drawtext` instead
---@param x integer
---@param y integer
---@param text string
function wgui.text(x, y, text) end

---Draws the text `text` at the specified coordinates and size. If the text is
---to large to fit in the rectangle specified in `rect`, it will wrap, unlike
---`wgui.text`. Uses the GDI font, GDI background, and GDI text color.
---@param text string The text to be drawn.
---@param rect {l: integer, t: integer, r: integer, b: integer}|{l: integer, t: integer, w: integer, h: integer} The bounding rectangle for the text. Can either be {l, t, r, b} (left, top, right, bottom) or {l, t, w, h} (left, top, width, height).
---@param format align_format? Each character in this string sets a formatting rule. It can be `nil`, `""`, or one or more formatting characters
function wgui.drawtext(text, rect, format) end

-- function wgui.rect() end

-- function wgui.fillrect() end

-- function wgui.fillrecta() end

-- function wgui.fillellipsea() end

-- function wgui.fillpolygona() end

-- function wgui.loadimage() end

-- function wgui.deleteimage() end

-- function wgui.drawimage() end

-- function wgui.loadscreen() end

-- function wgui.loadscreenreset() end

-- function wgui.getimageinfo() end

-- function wgui.ellipse() end

-- function wgui.polygon() end

-- function wgui.line() end

-- function wgui.info() end

-- function wgui.resize() end

--#endregion


-- input functions
--#region

---Returns the state of all keyboard keys and the mouse position in a table. Ex: `input.get() -> {xmouse=297, ymouse=120, A=true, B=true}`.
---@return table
function input.get() end

---Returns the differences between `t1` and `t2`. For example, if `t1` is the inputs for this frame, and `t2` is the inputs for last frame, it would return which buttons were pressed this frame, not which buttons are active.
---@param t1 table
---@param t2 table
---@return table
function input.diff(t1, t2) end

---Opens a window where the user can input text. If `OK` is clicked, that text is returned. If `Cancel` is clicked or the window is closed, `nil` is returned.
---@param title string? The title of the text box. Defaults to "input:".
---@param placeholder string? The text box is filled with this string when it opens. Defaults to "".
function input.prompt(title, placeholder) end

--#endregion


-- joypad functions
--#region

---Gets the currently pressed game buttons and stick direction for a given port. Note that the `y` coordinate of the stick is the opposite of what is shown on TAS Input.
---@param port 1|2|3|4
---@return table
function joypad.get(port) end

---Sets the current joypad to `inputs`. If you do not specify one or more inputs, they will be set to `false` for buttons or `0` for stick coordinates
---@param port 1|2|3|4
---@param inputs table
function joypad.set(port, inputs) end

---Calls the function `f` every input frame. The function `f` receives an
---argument that seems to always be `0`. If `unregister` is set to true, the
---function `f` will no longer be called when this event occurs, but it will
---error if you never registered the function. Alias for `emu.atinput`.
---@param f fun(a: integer?): nil The function to be called every input frame. It receives an argument that seems to always be `0`.
---@param unregister boolean? If true, then unregister the function `f`.
---@return nil
function joypad.register(f, unregister) end


---Returns the number of input frames that have happened since the emulator was
---started. It does not reset when a movie is started. Alias for `emu.inputcount`.
---@nodiscard
---@return integer inputcount The number of input frames that have happened since the emulator was started.
function joypad.count() end

--#endregion


-- movie functions
--#region

---Plays a movie file located at `filename`. This function sets `Read Only` to true.
---@param filename string
function movie.playmovie(filename) end

---Stops the currently playing movie.
function movie.stopmovie() end

---Returns the filename of the currently playing movie. It will error if no movie is playing.
---@return string
function movie.getmoviefilename() end

--#endregion


-- savestate functions
--#region

---Saves a savestate to `filename`.
---@param filename string
function savestate.savefile(filename) end

---Loads a savestate from `filename`
---@param filename string
function savestate.loadfile(filename) end

--#endregion


-- ioHelper functions
--#region

---Opens a file dialouge and returns the file path
---@param filter string
---@param type integer
---@return string
function ioHelper.filediag(filter, type) end

--#endregion


-- avi functions
--#region

---Begins an avi recording using the previously saved encoding settings. It is saved to `filename`.
---@param filename string
function avi.startcapture(filename) end

---Stops avi recording.
function avi.stopcapture() end
