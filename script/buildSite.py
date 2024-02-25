import json
import re

import markdown
import minify_html
from markdown.extensions.codehilite import CodeHiliteExtension
from markdown.extensions.fenced_code import FencedCodeExtension

# RUN "pip install -r requirements.txt" BEFORE RUNNING THIS SCRIPT. This script
# assumes the working directory is the base git directory (not /script). It is
# recommended that you run this with VS Code instead of the command line

# Command to generate pygments.css (run inside the css folder):
# pygmentize -S pastie -f html -a .codehilite > pygments.css

# TODO:


class StringAccumulator:
    def __init__(self):
        self.segments: list[str] = []

    def write(self, string: str | list[str]):
        if type(string) == list:
            self.segments.append(*string)
        else:
            self.segments.append(string)

    def retrieve(self, separator: str = "") -> str:
        return separator.join(self.segments)


def parse_markdown(str: str) -> str:
    return markdown.markdown(str, extensions=[CodeHiliteExtension(), FencedCodeExtension()])


# {"global": ["print", "stop"]}
def read_funcs_from_cpp_file() -> dict[str, list[str]]:
    func_type_pattern = re.compile(
        r'const luaL_Reg (?P<func_type>[A-Za-z0-9]+)Funcs\[\]')
    func_name_pattern = re.compile(r'\{"(?P<func_name>[A-Za-z0-9_]+)",.*\}')

    func_list_dict = {}

    # TODO: add error if file doesn't exist and remind to init submodule
    with open('mupen64-rr-lua-/lua/LuaConsole.cpp', 'r', encoding='utf-8') as file:
        # if we're far enough into the file to start caring
        in_function_region = False
        func_type = ''
        for l in file:
            line = l.strip()
            # Start at first line of Lua emu func arrays
            if "// begin lua funcs" in line:
                in_function_region = True
                continue
            # Stop at end of functions
            if "// end lua funcs" in line:
                break
            if in_function_region:
                # If we're iterating over a function name line...
                if '{"' in line and 'NULL' not in line:
                    func_name = func_name_pattern.search(
                        line).group('func_name')
                    # func_list_dict[func_type] should already exist
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

                extends = definition["extends"]
                variable_type = extends["type"]
                if variable_type == "function":

                    if not "." in variable_name:
                        variable_name = f"global.{variable_name}"

                    if not variable_name in functions:
                        functions[variable_name] = []

                    functions[variable_name].append({
                        "desc": extends["desc"],
                        "view": extends["view"]
                    })
    return functions


def generate_function_html(func_type, func_name, display_name, desc, view, example):
    if example == "":
        example_markdown = ""
    else:
        example_markdown = f'''
#### Example:
```lua
{example}
```
'''
    return parse_markdown(f'''
---

# {f'<a id="{func_type}{func_name.capitalize()}">'}{display_name}</a>

{desc}


```lua
{view}
```

{example_markdown}

''')


def main():
    skipped_functions = ["printx", "tostringex", "setgfx"]

    accumulator = StringAccumulator()

    accumulator.write("""
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

    used_lua_functions = []

    # loop over function types (emu, wgui)
    for func_type in cpp_functions:
        accumulator.write(
            f'''
            <div class="collapsible">
                <a href="#{func_type}Funcs">{func_type.upper()} FUNCTIONS</a>
            </div>
            ''')
        accumulator.write('<div class="funcList">')
        for func_name in cpp_functions[func_type]:
            if func_name in skipped_functions:
                continue

            display_name = func_name.upper(
            ) if func_type == "global" else f"{func_type.upper()}.{func_name.upper()}"

            accumulator.write(
                f'''
                <a href="#{func_type}{func_name.capitalize()}">
                    <button class="funcListItem" onclick="highlightFunc('{func_type}{func_name.capitalize()}')">{display_name}</button>
                </a>
                ''')
        accumulator.write('</div>')  # closes div.funcList
    accumulator.write('</div>')  # closes div.sidebar

    accumulator.write('<div class="docBody">')
    for func_type in cpp_functions:
        # create the section header

        accumulator.write(parse_markdown(
            f'---\n# <a id="{func_type}Funcs">{func_type.upper()}</a>FUNCTIONS'))

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
                    example = ""
                    example_filename = f"examples/{func_type}/{func_name}.lua"
                    try:
                        with open(example_filename, "rt") as f:
                            example = f.read()
                    except:
                        print(f"couldn't find example file {example_filename}")
                        pass
                    accumulator.write(
                        f'<div name="{func_type}{func_name.capitalize()}">')
                    accumulator.write(generate_function_html(
                        func_type, func_name, display_name, desc, view, example))
                    accumulator.write('</div>')
                used_lua_functions.append(fullname)
            else:
                print(f"c++ function {fullname} wasn't in lua functions")
                desc = "?"
                view = "?"
                accumulator.write(
                    f'<div name={func_type}{func_name.capitalize()}>')
                accumulator.write(generate_function_html(
                    func_type, func_name, display_name, desc, view, ""))
                accumulator.write('</div>')

    accumulator.write("</div>")  # closed div.docBody

    # make sure every lua function was used
    for key in lua_functions.keys():
        if not key in used_lua_functions:
            print(f"lua function wasn't used: {key}")

    # add javascript
    with open("script/index.js", "rt") as file:
        accumulator.write(f"<script>{file.read()}</script>")

    accumulator.write("</body></html>")

    with open("docs/index.html", "w+") as file:
        file.write(minify_html.minify(accumulator.retrieve(), keep_html_and_head_opening_tags=True,
                                      minify_js=True, do_not_minify_doctype=True,
                                      ensure_spec_compliant_unquoted_attribute_values=True,
                                      keep_closing_tags=True, minify_css=True,
                                      remove_processing_instructions=True))

    with open("docs/index-no-min.html", "w+") as file:
        file.write(accumulator.retrieve())


if __name__ == "__main__":
    main()
