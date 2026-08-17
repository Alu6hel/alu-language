import sys
import re

text = open('cpp_frontend/semantic_analyzer.cpp').read()

text = re.sub(
    r'(\s*if \(auto routine = dynamic_cast<RoutineNode\*>\(decl\)\) \{)(\s*checkRoutine\(routine\);\s*\} else if)',
    r'\1\n              if (!routine->type_params.empty()) continue;\2',
    text
)

open('cpp_frontend/semantic_analyzer.cpp', 'w').write(text)
