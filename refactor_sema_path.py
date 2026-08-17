import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

target = '''                std::string cycleStr = "";
                bool inCycle = false;
                std::string startNode = path.back();
                for (const auto& node : path) {
                    if (node == startNode) inCycle = true;
                    if (inCycle) {
                        cycleStr += node + " -> ";
                    }
                }
                cycleStr += startNode;'''

new_target = '''                std::string cycleStr = "";
                bool inCycle = false;
                std::string startNode = path.back();
                for (size_t i = 0; i < path.size() - 1; ++i) {
                    if (path[i] == startNode) inCycle = true;
                    if (inCycle) {
                        cycleStr += path[i] + " -> ";
                    }
                }
                cycleStr += startNode;'''

content = content.replace(target, new_target)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print("semantic_analyzer path generation fixed")
