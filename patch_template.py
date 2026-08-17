import re

text = open('cpp_frontend/semantic_analyzer.cpp').read()

# 1. Update checkDeclarations to store templates
pattern1 = r'void SemanticAnalyzer::checkDeclarations\(const std::vector<std::unique_ptr<ASTNode>>& declarations\) \{.*?for \(size_t i = 0; i < declarations\.size\(\); \+\+i\) \{.*?if \(auto routine = dynamic_cast<RoutineNode\*>\(decl\)\) \{.*?routine->name = prefixName\(routine->name\);'
replacement1 = '''void SemanticAnalyzer::checkDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations) {
    for (size_t i = 0; i < declarations.size(); ++i) {
        auto* decl = declarations[i].get();
        if (auto routine = dynamic_cast<RoutineNode*>(decl)) {
            routine->name = prefixName(routine->name);
            if (!routine->type_params.empty()) {
                routine_templates[routine->name] = routine;
                continue;
            }
'''
text = re.sub(pattern1, replacement1, text, flags=re.DOTALL)

# 2. Update checkDeclarationsSecondPass to skip templates
pattern2 = r'void SemanticAnalyzer::checkDeclarationsSecondPass\(const std::vector<std::unique_ptr<ASTNode>>& declarations\) \{.*?for \(size_t i = 0; i < declarations\.size\(\); \+\+i\) \{.*?if \(auto routine = dynamic_cast<RoutineNode\*>\(decl\)\) \{.*?checkRoutine\(routine\);'
replacement2 = '''void SemanticAnalyzer::checkDeclarationsSecondPass(const std::vector<std::unique_ptr<ASTNode>>& declarations) {
    for (size_t i = 0; i < declarations.size(); ++i) {
        auto* decl = declarations[i].get();
        if (auto routine = dynamic_cast<RoutineNode*>(decl)) {
            if (!routine->type_params.empty()) continue;
            checkRoutine(routine);'''
text = re.sub(pattern2, replacement2, text, flags=re.DOTALL)

# 3. Implement instantiateRoutineTemplateIfNeeded
pattern3 = r'void SemanticAnalyzer::instantiateRoutineTemplateIfNeeded\(const std::string& name, const std::vector<std::string>& type_args\) \{\}'
replacement3 = '''void SemanticAnalyzer::instantiateRoutineTemplateIfNeeded(const std::string& name, const std::vector<std::string>& type_args) {
    std::string mangledName = name;
    for (const auto& ta : type_args) mangledName += "_" + resolveName(ta);
    if (function_table.find(mangledName) != function_table.end()) return;
    
    if (routine_templates.find(name) == routine_templates.end()) return;
    
    RoutineNode* r = routine_templates[name];
    if (r->type_params.size() != type_args.size()) return;
    
    std::map<std::string, std::string> type_map;
    for (size_t i = 0; i < type_args.size(); ++i) {
        type_map[r->type_params[i]] = resolveName(type_args[i]);
    }
    
    auto cloned = r->clone(type_map);
    RoutineNode* instantiated = dynamic_cast<RoutineNode*>(cloned.get());
    if (instantiated) {
        instantiated->name = mangledName;
        instantiated->type_params.clear();
        
        FunctionSignature sig;
        sig.returnType = parseDataType(instantiated->returnType);
        for (auto& p : instantiated->params) {
            DataType t = parseDataType(p.type);
            sig.paramTypes.push_back(t);
        }
        sig.isVariadic = false;
        function_table[mangledName] = sig;
        
        current_ast->declarations.push_back(std::move(cloned));
    }
}'''
text = re.sub(pattern3, replacement3, text)

open('cpp_frontend/semantic_analyzer.cpp', 'w').write(text)
