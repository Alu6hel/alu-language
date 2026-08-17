with open('std/io.alu', 'a') as f:
    f.write('\nroutine println(string s) -> void {\n    printf("%s\\n", s);\n}\n\nroutine print_int(int i) -> void {\n    printf("%d", i);\n}\n')
