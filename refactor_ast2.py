import re

with open('cpp_frontend/ast.h', 'r') as f:
    ast_content = f.read()

# Update DataType enum
target = '''    FLOAT,
    DOUBLE,
    BYTE
};'''

new_target = '''    FLOAT,
    DOUBLE,
    BYTE,
    FLOAT4,
    FLOAT8,
    INT4,
    INT8
};'''

ast_content = ast_content.replace(target, new_target)

# Update DataTypeToString
target2 = '''        case DataType::BYTE: return "BYTE";
        default: return "UNKNOWN";
    }'''

new_target2 = '''        case DataType::BYTE: return "BYTE";
        case DataType::FLOAT4: return "FLOAT4";
        case DataType::FLOAT8: return "FLOAT8";
        case DataType::INT4: return "INT4";
        case DataType::INT8: return "INT8";
        default: return "UNKNOWN";
    }'''

ast_content = ast_content.replace(target2, new_target2)

with open('cpp_frontend/ast.h', 'w') as f:
    f.write(ast_content)

print("AST updated")
