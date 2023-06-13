import markdown
from markdown.extensions.codehilite import CodeHiliteExtension
from markdown.extensions.fenced_code import FencedCodeExtension
import json
import re
from bs4 import BeautifulSoup as bs

# RUN "pip install -r requirements.txt" BEFORE RUNNING THIS SCRIPT
# This script assumes the working directory is the base git directory (not /script)
# It is recommended that you run this with VS Code instead of the command line

# Command to generate pygments.css (run inside the css folder):
# pygmentize -S pastie -f html -a .codehilite > pygments.css

# TODO:


class Segments:
    def __init__(self):
        self.segments: list[str] = []

    def write(self, string: str):
        self.segments.append(string)

    def retrieve(self) -> str:
        return "".join(self.segments)


def parse_markdown(str: str) -> str:
    return markdown.markdown(str, extensions=[CodeHiliteExtension(), FencedCodeExtension()])


def read_funcs_from_cpp_file() -> dict[str, list[str]]:
    func_type_pattern = re.compile(
        r'const luaL_Reg (?P<func_type>[A-Za-z]+)Funcs\[\]')
    func_name_pattern = re.compile(r'\{"(?P<func_name>[A-Za-z0-9_]+)",.*\}')

    func_list_dict = {}

    with open('LuaConsole.cpp', 'r', encoding='utf-8') as file:
        # if we're far enough into the file to start caring
        in_function_region = False
        func_type = ''
        for line in file:
            # Start at first line of Lua emu func arrays
            # this would have to be changed to include global functions
            if line == '	const luaL_Reg globalFuncs[] = {\n':
                in_function_region = True
            # Stop at end of namespace
            if line == '}	//namespace\n':
                break
            if in_function_region:
                # If we're iterating over a function name line...
                if '{"' in line and 'NULL' not in line:
                    func_name = func_name_pattern.search(
                        line).group('func_name')
                    func_list_dict[func_type].append(func_name)
                # If we're iterating over a function list line...
                elif '[' in line:
                    func_type = func_type_pattern.search(
                        line).group('func_type')
                    func_list_dict[func_type] = []

    return func_list_dict


def read_funcs_from_json_file() -> dict[str, list[dict[str, str]]]:
    functions = {}
    # only accept definitions from this file
    api_filename_ending = "api.lua"

    with open("export/doc.json", "rt") as f:
        data = json.loads(f.read())
    # data is an array with all the variables
    for variable in data:
        # store the name of the variable
        variable_name = variable["name"]
        # print(variable_name)
        # each variable can have multiple definitions (print has 2, one is for mupen and one is the regular lua one)
        for definition in variable["defines"]:
            # get the source file name for every definition
            file_name = definition["file"]
            # only process the definition if it is one we want
            if file_name.endswith(api_filename_ending):

                if not "." in variable_name:
                    variable_name = f"global.{variable_name}"

                if not variable_name in functions:
                    functions[variable_name] = []

                extends = definition["extends"]
                variable_type = extends["type"]
                if variable_type == "function":
                    functions[variable_name].append({
                        "desc": extends["desc"],
                        "view": extends["view"]
                    })
    return functions

def generate_function_html(func_type, func_name, display_name, desc, view):
    return parse_markdown(f'''
---

# {f'<a id="{func_type}{func_name.capitalize()}">'}{display_name}</a>

{desc}


```lua
{view}
```


''')

def main():
    skipped_functions = ["printx", "tostringex"]

    segments = Segments()

    segments.write("""
    <!DOCTYPE html>
    <html lang="en">
        <head>
            <meta charset="UTF-8" />
            <meta http-equiv="X-UA-Compatible" content="IE=edge" />
            <meta name="viewport" content="width=device-width, initial-scale=1.0" />
            <link href="css/styles.css" type="text/css" rel="stylesheet">
            <link href="css/pygments.css" type="text/css" rel="stylesheet">
            <title>Mupen Lua API Docs</title>
        </head>
        <body>
            <div class="sidebar">
            <div class="sidebarheader">
                <div class="logo">
                    <a href="index.html"><img src="img/mupen_logo.png"></a>
                </div>
                <div class="logolabel">
                    mupen64-rr-lua docs
                </div>
            </div>
    """)

    cpp_functions = read_funcs_from_cpp_file()
    lua_functions = read_funcs_from_json_file()

    # loop over function types (emu, wgui)
    for func_type in cpp_functions:
        segments.write(
            f'''
            <div class="collapsible">
                <a href="#{func_type}Funcs">{func_type.upper()} FUNCTIONS</a>
            </div>
            ''')
        segments.write('<div class="funcList">')
        for func_name in cpp_functions[func_type]:
            if func_name in skipped_functions:
                continue

            display_name = func_name.upper(
            ) if func_type == "global" else f"{func_type.upper()}.{func_name.upper()}"

            segments.write(
                f'''
                <a href="#{func_type}{func_name.capitalize()}">
                    <button class="funcListItem" onclick="highlightFunc('{func_type}{func_name.capitalize()}')">{display_name}</button>
                </a>
                ''')
        segments.write('</div>')  # closes div.funcList
    segments.write('</div>')  # closes div.sidebar

    segments.write('<div class="docBody">')
    for func_type in cpp_functions:
        # create the section header

        segments.write(parse_markdown(
            f'---\n# <a id="{func_type}Funcs">{func_type.upper()}</a> FUNCTIONS'))

        for func_name in cpp_functions[func_type]:
            if func_name in skipped_functions:
                continue
            fullname = f"{func_type}.{func_name}"
            display_name = func_name if func_type == "global" else f"{func_type}.{func_name}"
            if fullname in lua_functions:
                lua_data = lua_functions[fullname]
                for var in lua_data:
                    desc = var["desc"]
                    view = var["view"]
                    segments.write(f'<div name={func_type}{func_name.capitalize()}>')
                    segments.write(generate_function_html(func_type, func_name, display_name, desc, view))
                    segments.write('</div>')
            else:
                print(f"{fullname} failed")
                desc = "?"
                view = "?"
                segments.write(f'<div name={func_type}{func_name.capitalize()}>')
                segments.write(generate_function_html(func_type, func_name, display_name, desc, view))
                segments.write('</div>')

    segments.write("</div>")  # closed div.docBody

    # add javascript
    with open("script/index.js", "rt") as file:
        segments.write(f"<script>{file.read()}</script>")

    with open("docs/index.html", "w+") as file:
        # run the html through beautiful soup to validate it and clean it up
        file.write(str(bs(segments.retrieve(), "html.parser")))


if __name__ == "__main__":
    main()
