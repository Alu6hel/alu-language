import re

text = open('cpp_frontend/llvm_codegen.cpp').read()

# the string literal parsing for FuncCallNode and ExternRoutine Call:
# We should probably just replace:
# if (auto lit = dynamic_cast<LiteralNode*>(node->args[i].get())) {
#     if (lit->type == DataType::STRING) {
#         std::string text = lit->value;
#         ...
#         continue;
#     }
# }

def replace_lit(text):
    pattern = r'if \(auto lit = dynamic_cast<LiteralNode\*>(.*?)\) \{\s*if \(lit->type == DataType::STRING\) \{.*?continue;\s*\}\s*\}'
    
    return re.sub(pattern, '', text, flags=re.DOTALL)

# Let's check how it looks
with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(replace_lit(text))

