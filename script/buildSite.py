import markdown
import json
import re
from bs4 import BeautifulSoup as bs

# TODO:
# - include global functions
# - (maybe) use something like beautifulsoup to generate html instead of just
#   strings

output_html = '''
<!DOCTYPE html>
<html>
<head>
    <title>Mupen Lua API Docs</title>
    <link href="css/styles.css" type="text/css" rel="stylesheet">
    <link href="css/pygments.css" type="text/css" rel="stylesheet">
    <meta charset="UTF-8">
</head>
<body>
    <div class="sidebar">
        <center><img src="img/mupen_logo.png"><br>
        mupen64-rr-lua docs</center><br>
'''


def write(str):
    # i know this isn't the best practice but i don't really care
    global output_html
    output_html += (str + "\n")


def read_funcs_from_cpp_file():
    func_type_pattern = re.compile(
        r'const luaL_Reg (?P<func_type>[A-Za-z]+)Funcs\[\]')
    func_name_pattern = re.compile(r'\{"(?P<func_name>[A-Za-z0-9]+)",.*\}')

    func_list_dict = {}

    with open('LuaConsole.cpp', 'r', encoding='utf-8') as file:
        # if we're far enough into the file to start caring
        in_function_region = False
        func_type = ''
        for line in file:
            # Start at first line of Lua emu func arrays
            # this would have to be changed to include global functions
            if line == 'const luaL_Reg emuFuncs[] = {\n':
                in_function_region = True
            # Stop at end of namespace
            if line == '}	//namespace\n':
                break
            if in_function_region:
                # If we're iterating over a function name line...
                if ',' in line and 'NULL' not in line:
                    func_name = func_name_pattern.search(
                        line).group('func_name')
                    func_list_dict[func_type].append(func_name)
                # If we're iterating over a function list line...
                elif '[' in line:
                    func_type = func_type_pattern.search(
                        line).group('func_type')
                    func_list_dict[func_type] = []

    return func_list_dict


def read_funcs_from_json_file():
    functions = {}
    # only accept definitions from this file
    api_filename_ending = "api.lua"

    with open("export/doc.json", "rt") as f:
        data = json.loads(f.read())
    # data is an array with all the variables
    for variable in data:
        # store the name of the variable
        variable_name = variable["name"]
        # each variable can have multiple definitions (print has 2, one is for mupen and one is the regular lua one)
        for definition in variable["defines"]:
            # get the source file name for every definition
            file_name = definition["file"]
            # only process the definition if it is one we want
            if file_name.endswith(api_filename_ending):
                extends = definition["extends"]
                variable_type = extends["type"]
                if variable_type == "function":
                    functions[variable_name] = {
                        "desc": extends["desc"],
                        "view": extends["view"]
                    }
    return functions


cpp_functions = read_funcs_from_cpp_file()
lua_functions = read_funcs_from_json_file()


# loop over function types (emu, wgui)
for func_type in cpp_functions:
    write(
        f'''
        <button class="collapsible">
            <a href="#{func_type}Funcs">{func_type.upper()} FUNCTIONS</a>
        </button>
        ''')
    write('<div class="funcList">')
    for func_name in cpp_functions[func_type]:
        write(
            f'''
            <button class="funcListItem">
                <a href="#{func_type}{func_name.capitalize()}">{func_type.upper()}.{func_name.upper()}</a>
            </button>
            ''')
    write('</div>')  # closes div.funcList
write('</div>')  # closes div.sidebar

write('<div class="docBody">')
for func_type in cpp_functions:
    # create the section header

    write(markdown.markdown(
        f'---\n# <a id="{func_type}Funcs">{func_type.upper()}</a> FUNCTIONS',
        extensions=['fenced_code', 'codehilite']))

    for func_name in cpp_functions[func_type]:
        fullname = f"{func_type}.{func_name}"
        if fullname in lua_functions:
            lua_data = lua_functions[fullname]
            desc = lua_data["desc"]
            view = lua_data["view"]
        else:
            print(f"{fullname} failed")
            desc = "?"
            view = "?"
        variable_anchor = f'<a id="{func_type}{func_name.capitalize()}">'

        write(markdown.markdown(f'''
---

# {variable_anchor}{fullname}</a>

{desc}


```lua
{view}
```


''', extensions=['fenced_code', 'codehilite']))


write("</div>")  # closed div.docBody


write('''<script>
    var coll = document.getElementsByClassName("collapsible");
    var i;
    
    for (i = 0; i < coll.length; i++) {
      coll[i].addEventListener("click", function() {
        this.classList.toggle("active");
        var content = this.nextElementSibling;
        if (content.style.maxHeight){
          content.style.maxHeight = null;
        } else {
          content.style.maxHeight = content.scrollHeight + "px";
        } 
      });
    }
</script>''')


with open("docs/index.html", "w+") as file:
    # run the html through beautiful soup to validate it and clean it up
    file.write(bs(output_html, "html.parser").prettify())
