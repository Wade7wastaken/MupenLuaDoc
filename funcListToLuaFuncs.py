import re

func_type_pattern = re.compile(r'const luaL_Reg (?P<func_type>[A-Za-z]+)Funcs\[\]')
func_name_pattern = re.compile(r'\{"(?P<func_name>[A-Za-z0-9]+)",.*\}')

func_list_dict = {}
with open('lua\\LuaConsole.cpp', 'r', encoding='utf-8') as file:
    start_append = False
    func_type = ''
    for line in file:
        # Start at first line of Lua emu func arrays
        if line == 'const luaL_Reg emuFuncs[] = {\n':
            start_append = True
        # Stop at end of namespace
        if line == '}	//namespace\n':
            break
        if start_append:
            # If we're iterating over a function name line...
            if ',' in line and 'NULL' not in line:
                func_name = func_name_pattern.search(line).group('func_name')
                func_list_dict[func_type].append(func_name)
            # If we're iterating over a function list line...
            elif '[' in line:
                func_type = func_type_pattern.search(line).group('func_type')
                func_list_dict[func_type] = []

with open('lua\\api.lua', 'w+') as file:
    for func_type in func_list_dict:
        for func_name in func_list_dict[func_type]:
            file.write(f'function {func_type}.{func_name}() end\n\n')