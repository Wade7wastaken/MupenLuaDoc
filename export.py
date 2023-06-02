import json

api_filename_ending = "api_wade7.lua"


def write(strings, f):
    for string in strings:
        f.write(f"{string}\n")


with open("export/doc.json", "rt") as f:
    data = json.loads(f.read())

with open("index.md", "wt") as f:
    for variable in data:
        variable_name = variable["name"]
        for definition in variable["defines"]:
            file_name = definition["file"]
            if file_name.endswith(api_filename_ending):
                variable_type = definition["extends"]["type"]
                if variable_type == "function":

                    write([
                        "---",
                        "",
                        f"# {variable_name}",
                        "",
                        definition["extends"]["desc"],
                        "",
                        "",
                        "```lua",
                        definition['extends']['view'],
                        "```"
                        "",
                        "",
                    ], f)
