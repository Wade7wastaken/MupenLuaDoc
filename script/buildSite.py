import json
import re

import markdown
import minify_html
from markdown.extensions.codehilite import CodeHiliteExtension
from markdown.extensions.fenced_code import FencedCodeExtension
from dataclasses import dataclass


# RUN "pip install -r requirements.txt" BEFORE RUNNING THIS SCRIPT. This script
# assumes the working directory is the base git directory (not /script). It is
# recommended that you run this with VS Code instead of the command line

# Command to generate pygments.css (run inside the css folder):
# pygmentize -S pastie -f html -a .codehilite > pygments.css

# TODO:
# - use a proper templating engine instead of string accumulation


class StringAccumulator:
    def __init__(self):
        self.segments: list[str] = []

    def write(self, string: str | list[str]):
        if isinstance(string, list):
            self.segments.append(*string)
        else:
            self.segments.append(string)

    def retrieve(self, separator: str = "") -> str:
        return separator.join(self.segments)

@dataclass(unsafe_hash=True)
class LuaFunc:
    module: str
    name: str

    # (global, print) -> print
    # (wgui, drawtext) -> wgui.drawtext
    def display_name(self) -> str:
        return self.name if self.module == "global" else f"{self.module}.{self.name}"
    
    # (global, print) -> globalPrint
    # (wgui, drawtext) -> wguiDrawtext
    def internal_name(self) -> str:
        return f"{self.module}{self.name.capitalize()}"

def from_lua_signature(fn: str) -> LuaFunc:
    segments = fn.split('.')
    if len(segments) == 1:
        segments.insert(0, "global")
    return LuaFunc(*segments)

@dataclass
class LuaFuncData:
    description: str
    view: str
    deprecated: bool


def parse_markdown(str: str) -> str:
    return markdown.markdown(
        str, extensions=[CodeHiliteExtension(), FencedCodeExtension()]
    )


def read_funcs_from_cpp_file(path: str) -> dict[str, list[str]]:
    func_type_pattern = re.compile(
        r"const luaL_Reg (?P<func_type>[A-Za-z0-9]+)_FUNCS\[\] = \{"
    )
    func_name_pattern = re.compile(r'\{"(?P<func_name>[A-Za-z0-9_]+)",.*\}')

    func_list_dict: dict[str, list[str]] = {}

    try:
        with open(path, "r", encoding="utf-8") as file:
            # if we're far enough into the file to start caring
            in_function_region = False
            func_type = ""
            for l in file:
                line = l.strip()
                # Start at first line of Lua emu func arrays
                if "// begin lua funcs" in line:
                    in_function_region = True
                    continue
                # Stop at end of functions
                if "// end lua funcs" in line:
                    break
                if not in_function_region:
                    continue

                # If we're iterating over a function name line...
                if '{"' in line and "NULL" not in line:
                    func_name_match = func_name_pattern.search(line)
                    if func_name_match is None:
                        raise Exception(f"Couldn't find a function name in line {line}")

                    func_name = func_name_match.group("func_name")
                    if not isinstance(func_name, str):
                        raise Exception(f"func_name wasn't a string in line {line}")
                    # func_list_dict[func_type] should already exist
                    func_list_dict[func_type].append(func_name)
                # If we're iterating over a function list line...
                elif "[" in line:
                    func_type_match = func_type_pattern.search(line)
                    if func_type_match is None:
                        raise Exception(f"Couldn't find a function name in line {line}")

                    func_type = func_type_match.group("func_type").lower()
                    func_list_dict[func_type] = []

        return func_list_dict
    except FileNotFoundError:
        print("Couldn't find LuaRegistry.cpp. Have you initialized the submodule?")
        exit(1)

def read_funcs_from_json_file(path: str, api_filename: str) -> dict[LuaFunc, list[LuaFuncData]]:
    functions: dict[LuaFunc, list[LuaFuncData]] = {}
    # only accept definitions from this file

    try:
        with open(path, "rt") as f:
            data = json.loads(f.read())
    except FileNotFoundError:
        print(
            "Couldn't find doc.json. Have you exported the documentation to the correct place?"
        )
        exit(1)

    # data is an array with all the variables
    for variable in data:
        if "DOC" in variable:
            assert variable["DOC"].endswith(api_filename)
            continue

        # store the name of the variable
        variable_name = variable["name"]

        # each variable can have multiple definitions (print has 2, one is for mupen and one is the regular lua one)
        for definition in variable["defines"]:
            if not "file" in definition:
                continue
            file_name = definition["file"]

            # only process the definition if it is one we want
            if file_name != ".":
                continue

            extends = definition["extends"]

            if extends["type"] != "function":
                continue

            fn = from_lua_signature(variable_name)

            if not fn in functions:
                functions[fn] = []

            func_data = LuaFuncData(extends["desc"], extends["view"], definition["deprecated"])
            functions[fn].append(func_data)
    return functions

def read_api_file(path: str) -> tuple[dict[int, LuaFunc], dict[LuaFunc, str]]:
    try:
        with open(path, "rt") as f:
            lines = [line.strip() for line in f]
    except FileNotFoundError:
        print("Couldn't find api file. Have you initialized the submodule?")
        exit(1)
    
    line_nums: dict[int, LuaFunc] = {}
    deprecation_messages: dict[LuaFunc, str] = {}

    deprecation_message = None

    for (i, l) in enumerate(lines):
        if l.startswith(f"---@deprecated "):
            deprecation_message = l.removeprefix("---@deprecated ").strip()

        if l.startswith("function "):
            end = l.find('(')
            fn = from_lua_signature(l[9:end].strip())

            if deprecation_message is not None:
                deprecation_messages[fn] = deprecation_message
                deprecation_message = None

            line_nums[i + 1] = fn

    return (line_nums, deprecation_messages)


def generate_function_html(fn: LuaFunc, desc: str, view: str, example: str | None, deprecated_message: str | None) -> str:
    if example is None:
        example_markdown = ""
    else:
        example_markdown = f"""
#### Example:
```lua
{example}
```
"""

    if deprecated_message is not None:
        deprecated_markdown = f'<span style="background-color: red; padding: 4px">This function is deprecated. {deprecated_message}</span>'
    else:
        deprecated_markdown = ""

    return parse_markdown(
        f"""
---

# {f'<a id="{fn.internal_name()}">'}{fn.display_name()}</a>

{deprecated_markdown}

{desc}


```luau
{view}
```

{example_markdown}

"""
    )


def handle_links(text: str, line_nums: dict[int, LuaFunc]) -> str:
    markdown_link_pattern = re.compile(r"\[(?P<link_text>.+)\]\(.*mupenapi\.lua\#(?P<line_num>\d+)\)")

    def a(x: re.Match[str]):
        line_num = x.group("line_num")
        if not isinstance(line_num, str):
            print("line num wasn't a string")
            exit(1)
        link_text = x.group("link_text")
        if not isinstance(link_text, str):
            print("link text wasn't a string")
            exit(1)
        fn = line_nums[int(line_num)]
        return f"[{link_text}](#{fn.internal_name()})"

    return re.sub(markdown_link_pattern, a, text)


def main():
    # Config
    api_filepath = "mupen64-rr-lua/src/api.lua"
    cpp_filepath = "mupen64-rr-lua/src/Views.Win32/lua/LuaRegistry.cpp"
    docs_filepath = "export/doc.json"
    skipped_functions: list[LuaFunc] = []

    cpp_functions = read_funcs_from_cpp_file(cpp_filepath)
    lua_functions = read_funcs_from_json_file(docs_filepath, api_filepath.split('/')[-1])
    (line_nums, deprecation_messages) = read_api_file(api_filepath)

    cpp_functions["os"] = ["exit"]

    html = StringAccumulator()

    html.write(
        """
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
    """
    )

    used_lua_functions = []

    # loop over function types (emu, wgui)
    for module in cpp_functions:
        html.write(
            f"""
            <div class="collapsible">
                <a href="#{module}Funcs">{module.upper()} FUNCTIONS</a>
            </div>
            """
        )
        html.write('<div class="funcList">')
        for name in cpp_functions[module]:
            fn = LuaFunc(module, name)
            if fn in skipped_functions:
                continue

            internal_name = fn.internal_name()
            display_name = fn.display_name().upper()

            html.write(
                f"""
                <a href="#{internal_name}">
                    <button class="funcListItem" onclick="highlightFunc('{internal_name}')">{display_name}</button>
                </a>
                """
            )
        html.write("</div>")  # closes div.funcList
    html.write("</div>")  # closes div.sidebar

    html.write('<div class="docBody">')
    for module in cpp_functions:
        # create the section header

        html.write(
            parse_markdown(
                f'---\n# <a id="{module}Funcs">{module.upper()}</a> FUNCTIONS'
            )
        )

        for name in cpp_functions[module]:
            fn = LuaFunc(module, name)

            if fn in skipped_functions:
                continue

            if fn in lua_functions:
                lua_data = lua_functions[fn]
                for var in lua_data:
                    description = handle_links(var.description, line_nums)
                    view = var.view

                    deprecated_message = None

                    if var.deprecated:
                        deprecated_message = handle_links(deprecation_messages[fn], line_nums)

                    example = None
                    example_filename = f"examples/{module}/{name}.lua"
                    try:
                        with open(example_filename, "rt") as f:
                            example = f.read()
                    except:
                        pass
                        # print(f"couldn't find example file {example_filename}")
                    html.write(
                        f'<div name="{fn.internal_name()}">'
                    )
                    html.write(
                        generate_function_html(
                            fn, description, view, example, deprecated_message
                        )
                    )
                    html.write("</div>")
                used_lua_functions.append(fn)
            else:
                print(f"c++ function {fn.display_name()} wasn't in lua functions")
                description = "?"
                view = "?"
                html.write(f"<div name={fn.internal_name()}>")
                html.write(
                    generate_function_html(
                        fn, description, view, None, None
                    )
                )
                html.write("</div>")

    html.write("</div>")  # closed div.docBody

    # make sure every lua function was used
    for key in lua_functions.keys():
        if not key in used_lua_functions:
            print(f"lua function wasn't used: {key}")

    # add javascript
    with open("script/index.js", "rt") as file:
        html.write(f"<script>{file.read()}</script>")

    html.write("</body></html>")

    with open("docs/index.html", "w+") as file:
        file.write(
            minify_html.minify(
                html.retrieve(),
                keep_html_and_head_opening_tags=True,
                minify_css=True,
                keep_closing_tags=True,
                remove_processing_instructions=True,
            )
        )

    with open("docs/index-no-min.html", "w+") as file:
        file.write(html.retrieve())

if __name__ == "__main__":
    main()
