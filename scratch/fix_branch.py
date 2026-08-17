import sys

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

# For IfNode
if_old = '''std::string cond_reg = evaluateExpression(node->condition.get());
      
      std::string then_label = getLabel("if.then");'''
if_new = '''std::string cond_reg = evaluateExpression(node->condition.get());
      std::string cond_type = getInferredLLVMType(node->condition.get());
      if (cond_type == "") cond_type = "i32";
      if (cond_type != "i1") {
          std::string bool_reg = getTempReg();
          emit("  " + bool_reg + " = icmp ne " + cond_type + " " + cond_reg + ", 0");
          cond_reg = bool_reg;
      }
      
      std::string then_label = getLabel("if.then");'''

content = content.replace(if_old, if_new)

# For WhileNode
while_old = '''std::string cond_reg = evaluateExpression(node->condition.get());
      
      emit("  br i1 " + cond_reg + ", label %" + body_label + ", label %" + end_label);'''
while_new = '''std::string cond_reg = evaluateExpression(node->condition.get());
      std::string cond_type = getInferredLLVMType(node->condition.get());
      if (cond_type == "") cond_type = "i32";
      if (cond_type != "i1") {
          std::string bool_reg = getTempReg();
          emit("  " + bool_reg + " = icmp ne " + cond_type + " " + cond_reg + ", 0");
          cond_reg = bool_reg;
      }
      
      emit("  br i1 " + cond_reg + ", label %" + body_label + ", label %" + end_label);'''

content = content.replace(while_old, while_new)

# For ForNode (if it exists)
for_old = '''std::string cond_reg = evaluateExpression(node->condition.get());
          emit("  br i1 " + cond_reg + ", label %" + body_label + ", label %" + end_label);'''
for_new = '''std::string cond_reg = evaluateExpression(node->condition.get());
          std::string cond_type = getInferredLLVMType(node->condition.get());
          if (cond_type == "") cond_type = "i32";
          if (cond_type != "i1") {
              std::string bool_reg = getTempReg();
              emit("  " + bool_reg + " = icmp ne " + cond_type + " " + cond_reg + ", 0");
              cond_reg = bool_reg;
          }
          emit("  br i1 " + cond_reg + ", label %" + body_label + ", label %" + end_label);'''

content = content.replace(for_old, for_new)

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)
