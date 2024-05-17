@echo off

:: run this script where api.lua is located

:: get the path of the lua language server
for /d %%D in ("%USERPROFILE%\.vscode\extensions\sumneko.lua*") do (
	set serverpath=%%D\server
)

:: generate the documentation using the current direcotry to find api.lua
%serverpath%\bin\lua-language-server.exe --doc "%~dp0api.lua"

:: copy the documentation to the export directory
move "%serverpath%\log\doc.json" "%~dp0export\doc.json"
move "%serverpath%\log\doc.md" "%~dp0export\doc.md"
