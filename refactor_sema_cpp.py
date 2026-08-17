import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

target = '''void SemanticAnalyzer::analyze(ProgramNode* ast) {
    if (!is_lsp_mode) std::cout << "[ALU CXX] Running Semantic Analysis..." << std::endl;
    current_ast = ast;
    checkProgram(ast);
    if (!is_lsp_mode) std::cout << "[ALU CXX] Semantic Analysis Passed: Memory and Type Safety verified." << std::endl;
}'''

new_target = '''void SemanticAnalyzer::analyze(ProgramNode* ast) {
    if (!is_lsp_mode) std::cout << "[ALU CXX] Running Semantic Analysis..." << std::endl;
    current_ast = ast;
    checkProgram(ast);
    analyzeOwnership();
    if (!is_lsp_mode) std::cout << "[ALU CXX] Semantic Analysis Passed: Memory and Type Safety verified." << std::endl;
}

bool SemanticAnalyzer::detectCycle(const std::string& current, std::unordered_set<std::string>& visited, std::unordered_set<std::string>& recStack, std::vector<std::string>& path, std::unordered_map<std::string, std::vector<std::string>>& adj) {
    if (recStack.find(current) != recStack.end()) {
        path.push_back(current);
        return true;
    }
    if (visited.find(current) != visited.end()) {
        return false;
    }
    
    visited.insert(current);
    recStack.insert(current);
    path.push_back(current);
    
    for (const auto& neighbor : adj[current]) {
        if (detectCycle(neighbor, visited, recStack, path, adj)) {
            return true;
        }
    }
    
    path.pop_back();
    recStack.erase(current);
    return false;
}

void SemanticAnalyzer::analyzeOwnership() {
    std::unordered_map<std::string, std::vector<std::string>> adj;
    
    for (const auto& kv : struct_table) {
        const std::string& structName = kv.first;
        for (const auto& field : kv.second.fields) {
            std::string type = field.type;
            if (type.find("managed<") == 0) {
                size_t p1 = type.find("<");
                size_t p2 = type.rfind(">");
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    std::string innerType = type.substr(p1 + 1, p2 - p1 - 1);
                    // clean up whitespace
                    while (!innerType.empty() && innerType.back() == ' ') innerType.pop_back();
                    while (!innerType.empty() && innerType.front() == ' ') innerType.erase(0, 1);
                    
                    if (struct_table.find(innerType) != struct_table.end() || struct_templates.find(innerType) != struct_templates.end()) {
                        adj[structName].push_back(innerType);
                    }
                }
            }
        }
    }
    
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recStack;
    std::vector<std::string> path;
    
    for (const auto& kv : struct_table) {
        if (visited.find(kv.first) == visited.end()) {
            if (detectCycle(kv.first, visited, recStack, path, adj)) {
                std::string cycleStr = "";
                bool inCycle = false;
                std::string startNode = path.back();
                for (const auto& node : path) {
                    if (node == startNode) inCycle = true;
                    if (inCycle) {
                        cycleStr += node + " -> ";
                    }
                }
                cycleStr += startNode;
                throw std::runtime_error("Ownership Error: Cyclical reference detected: " + cycleStr + ". Cyclical references prevent ARC deallocation and cause memory leaks. Use weak references or redesign your data layout.");
            }
        }
    }
}'''

content = content.replace(target, new_target)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print("semantic_analyzer.cpp updated")
