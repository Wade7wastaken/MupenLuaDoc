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


-- emu functions
--#region

---Displays the text message in the console.
---Similar to `print`, but only accepts strings
---@param message string
---@return nil
function emu.console(message) end

---Seems to do nothing.
---@return nil
function emu.debugview() end

---Displays the text `message` in the status bar
---on the bottom while replacing any other text.
---The message will only display until the next frame.
---@param message string
---@return nil
function emu.statusbar(message) end

---Calls the function `f` every vi frame. For example,
---in Super Mario 64, the function will be called twice
---when you advance by one frame whereas it will be called
---once in Ocarina of Time. If `unregister` is set to true,
---the function `f` will no longer be called when this event occurs,
---but it will error if you never registered the function.
---@param f fun(nil)
---@param unregister boolean?
---@return nil
function emu.atvi(f, unregister) end

---Seems similar to `emu.atvi`, except that it is called after.
---If `unregister` is set to true, the function `f` will no longer
---be called when this event occurs, but it will error if you
---never registered the function.
---@param f fun(nil)
---@param unregister boolean?
---@return nil
function emu.atupdatescreen(f, unregister) end

---Calls the function `f` every input frame. The function `f` receives
---the argument `a` that seems to always be `0`. If `unregister` is set
---to true, the function `f` will no longer be called when this event
---occurs, but it will error if you never registered the function.
---@param f fun(a: integer?)
---@param unregister boolean?
---@return nil
function emu.atinput(f, unregister) end

---Calls the function `f` when the script is stopped. If `unregister` is
---set to true, the function `f` will no longer be called when this event
---occurs, but it will error if you never registered the function.
---@param f fun(nil)
---@param unregister boolean?
---@return nil
function emu.atstop(f, unregister) end

---Defines a handler function that is called when a window receives a
---message. If `unregister` is set to true, the function `f` will no longer
---be called when this event occurs, but it will error if you never
---registered the function.
---@param f fun(a: 0) takes the following parameters:
function emu.atwindowmessage(f, unregister) end

---Calls the function `f` constantly, even when the emulator is paused
---If `unregister` is set to true, the function `f` will no longer
---be called when this event occurs, but it will error if you never
---registered the function.
---@param f fun(nil)
---@param unregister boolean?
function emu.atinterval(f, unregister) end

---Calls the function `f` when a movie is played. If `unregister`
---is set to true, the function `f` will no longer
---be called when this event occurs, but it will error if you never
---registered the function.
---@param f any
---@param unregister any
function emu.atplaymovie(f, unregister) end

-- function emu.atstopmovie() end

-- function emu.atloadstate() end

-- function emu.atsavestate() end

-- function emu.atreset() end

-- function emu.framecount() end

-- function emu.samplecount() end

-- function emu.inputcount() end

-- function emu.getversion() end

-- function emu.pause() end

-- function emu.getpause() end

-- function emu.getspeed() end

-- function emu.speed() end

-- function emu.speedmode() end

-- function emu.setgfx() end

-- function emu.getaddress() end

-- function emu.isreadonly() end

-- function emu.getsystemmetrics() end

-- function emu.ismainwindowinforeground() end

-- function emu.screenshot() end

-- function memory.LBU() end

-- function memory.LB() end

-- function memory.LHU() end

-- function memory.LH() end

-- function memory.LWU() end

-- function memory.LW() end

-- function memory.LDU() end

-- function memory.LD() end

-- function memory.LWC1() end

-- function memory.LDC1() end

-- function memory.loadsize() end

-- function memory.loadbytes() end

-- function memory.loadhalfs() end

-- function memory.loadwords() end

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

-- function gui.register() end

-- function wgui.setbrush() end

-- function wgui.setpen() end

-- function wgui.setcolor() end

-- function wgui.setbk() end

-- function wgui.setfont() end

-- function wgui.text() end

-- function wgui.drawtext() end

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

-- function input.get() end

-- function input.diff() end

-- function input.prompt() end

-- function joypad.get() end

-- function joypad.set() end

-- function joypad.register() end

-- function joypad.count() end

-- function movie.playmovie() end

-- function movie.stopmovie() end

-- function movie.getmoviefilename() end

-- function savestate.savefile() end

-- function savestate.loadfile() end

-- function ioHelper.filediag() end

-- function avi.startcapture() end

-- function avi.stopcapture() end
