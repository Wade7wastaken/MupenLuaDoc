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

    def add(self, string: str | list[str]):
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
    module_pattern = re.compile(
        r"const luaL_Reg (?P<module>[A-Za-z0-9]+)_FUNCS\[\] = \{"
    )
    name_pattern = re.compile(r'\{"(?P<name>[A-Za-z0-9_]+)",.*\}')

    func_list_dict: dict[str, list[str]] = {}

    try:
        with open(path, "rt", encoding="utf-8") as file:
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
                    func_name_match = name_pattern.search(line)
                    if func_name_match is None:
                        raise Exception(f"Couldn't find a function name in line {line}")

                    func_name = func_name_match.group("name")
                    if not isinstance(func_name, str):
                        raise Exception(f"func_name wasn't a string in line {line}")

                    # func_list_dict[func_type] should already exist
                    func_list_dict[func_type].append(func_name)
                # If we're iterating over a function list line...
                elif "[" in line:
                    func_type_match = module_pattern.search(line)
                    if func_type_match is None:
                        raise Exception(f"Couldn't find a function name in line {line}")

                    func_type = func_type_match.group("module").lower()
                    func_list_dict[func_type] = []

        return func_list_dict
    except FileNotFoundError:
        print("Couldn't find LuaRegistry.cpp. Have you initialized the submodule?")
        exit(1)

def read_funcs_from_json_file(path: str, api_filename: str) -> dict[LuaFunc, list[LuaFuncData]]:
    functions: dict[LuaFunc, list[LuaFuncData]] = {}

    try:
        with open(path, "rt", encoding="utf-8") as f:
            data = json.loads(f.read())
    except FileNotFoundError:
        print(
            "Couldn't find doc.json. Have you exported the documentation to the correct place?"
        )
        exit(1)

    # data is an array with all the variables
    for variable in data:
        # only accept files exported from the api file
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

            # only process the definitions from this file
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
        with open(path, "rt", encoding="utf-8") as f:
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


def generate_example_markdown(example: str | None):
    if example is None:
        return ""

    return f"""
#### Example:
```lua
{example}
```
"""

def generate_deprecated_markdown(deprecated_message: str | None):
    if deprecated_message is None:
        return ""

    return f'<span style="background-color: red; padding: 4px">This function is deprecated. {deprecated_message}</span>'

def generate_function_html(fn: LuaFunc, desc: str, view: str, deprecated_message: str | None, example: str | None) -> str:
    example_markdown = generate_example_markdown(example)
    deprecated_markdown = generate_deprecated_markdown(deprecated_message)

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


def transform_links(text: str, line_nums: dict[int, LuaFunc]) -> str:
    markdown_link_pattern = re.compile(r"\[(?P<link_text>.+)\]\(.*mupen.*\.lua\#(?P<line_num>\d+)\)")

    def replace(x: re.Match[str]):
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

    return re.sub(markdown_link_pattern, replace, text)


def add_header(html: StringAccumulator):
    html.add(
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

def add_sidebar(html: StringAccumulator, cpp_functions: dict[str, list[str]], skipped_functions: list[LuaFunc]):
    # loop over function types (emu, wgui)
    for module in cpp_functions:
        html.add(
            f"""
            <div class="collapsible">
                <a href="#{module}Funcs">{module.upper()} FUNCTIONS</a>
            </div>
            """
        )
        html.add('<div class="funcList">')
        for name in cpp_functions[module]:
            fn = LuaFunc(module, name)
            if fn in skipped_functions:
                continue

            internal_name = fn.internal_name()
            display_name = fn.display_name().upper()

            html.add(
                f"""
                <a href="#{internal_name}">
                    <button class="funcListItem" onclick="highlightFunc('{internal_name}')">{display_name}</button>
                </a>
                """
            )
        html.add("</div>")  # closes div.funcList
    html.add("</div>")  # closes div.sidebar

def load_example(fn: LuaFunc) -> str | None:
    example_filename = f"examples/{fn.module}/{fn.name}.lua"
    try:
        with open(example_filename, "rt", encoding="utf-8") as f:
            return f.read()
    except:
        # print(f"couldn't find example file {example_filename}")
        return None

def add_body(
        html: StringAccumulator,
        cpp_functions: dict[str, list[str]],
        lua_functions: dict[LuaFunc, list[LuaFuncData]],
        skipped_functions: list[LuaFunc],
        line_nums: dict[int, LuaFunc],
        deprecation_messages: dict[LuaFunc, str],
    ):
    used_lua_functions = []

    html.add('<div class="docBody">')
    for module in cpp_functions:
        # create the section header

        html.add(
            parse_markdown(
                f'---\n# <a id="{module}Funcs">{module.upper()}</a> FUNCTIONS'
            )
        )

        for name in cpp_functions[module]:
            fn = LuaFunc(module, name)

            if fn in skipped_functions:
                continue

            if fn not in lua_functions:
                print(f"c++ function {fn.display_name()} wasn't in lua functions")
                html.add(f"<div name={fn.internal_name()}>")
                html.add(generate_function_html(fn, "?", "?", None, None))
                html.add("</div>")
                continue

            lua_data = lua_functions[fn]
            for var in lua_data:
                description = transform_links(var.description, line_nums)

                deprecated_message = None
                if var.deprecated:
                    deprecated_message = transform_links(deprecation_messages[fn], line_nums)

                example = load_example(fn)

                html.add(f'<div name="{fn.internal_name()}">')
                html.add(generate_function_html(fn, description, var.view, deprecated_message, example))
                html.add("</div>")
            used_lua_functions.append(fn)

    html.add("</div>")  # closed div.docBody

    # make sure every lua function was used
    for key in lua_functions.keys():
        if not key in used_lua_functions:
            print(f"lua function wasn't used: {key}")

def add_javascript(html: StringAccumulator):
    # add javascript
    with open("script/index.js", "rt", encoding="utf-8") as file:
        html.add(f"<script>{file.read()}</script>")

def add_footer(html: StringAccumulator):
    html.add("</body></html>")

def write_output(data: str):
    with open("docs/index.html", "w", encoding="utf-8") as file:
        file.write(
            minify_html.minify(
                data,
                keep_html_and_head_opening_tags=True,
                minify_css=True,
                keep_closing_tags=True,
                remove_processing_instructions=True,
            )
        )

    with open("docs/index-no-min.html", "w", encoding="utf-8") as file:
        file.write(data)

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

    add_header(html)
    add_sidebar(html, cpp_functions, skipped_functions)
    add_body(html, cpp_functions, lua_functions, skipped_functions, line_nums, deprecation_messages)
    add_javascript(html)
    add_footer(html)
    write_output(html.retrieve())

if __name__ == "__main__":
    main()
