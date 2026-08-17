import re

with open('cpp_frontend/semantic_analyzer.h', 'r') as f:
    content = f.read()

target = '''    void checkProgram(ProgramNode* node);
    void checkDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    void checkDeclarationsSecondPass(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    
public:
    void analyze(ProgramNode* ast);'''

new_target = '''    void checkProgram(ProgramNode* node);
    void checkDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    void checkDeclarationsSecondPass(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    
    void analyzeOwnership();
    bool detectCycle(const std::string& current, std::unordered_set<std::string>& visited, std::unordered_set<std::string>& recStack, std::vector<std::string>& path, std::unordered_map<std::string, std::vector<std::string>>& adj);
    
public:
    void analyze(ProgramNode* ast);'''

content = content.replace(target, new_target)

# Wait, we need std::unordered_set in semantic_analyzer.h
if '#include <unordered_set>' not in content:
    target2 = '''#include <unordered_map>'''
    new_target2 = '''#include <unordered_map>\n#include <unordered_set>'''
    content = content.replace(target2, new_target2)

with open('cpp_frontend/semantic_analyzer.h', 'w') as f:
    f.write(content)

print("semantic_analyzer.h updated")
