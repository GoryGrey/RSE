#!/usr/bin/env python3

# Script to fix grey_compiler build issues

import re

# Fix types.rs
print("Fixing types.rs...")

with open('crates/grey_lang/src/types.rs', 'r') as f:
    types_content = f.read()

# Fix 1: Remove unused DiagnosticError import
types_content = re.sub(r'use crate::diagnostics::\{Diagnostic, DiagnosticError\};', 
                      'use crate::diagnostics::Diagnostic;', 
                      types_content)

# Fix 2: Add & to param.param_type
types_content = re.sub(r'self\.convert_ast_type\(param\.param_type\)\?;',
                      'self.convert_ast_type(¶m.param_type)?;',
                      types_content)

with open('crates/grey_lang/src/types.rs', 'w') as f:
    f.write(types_content)

# Fix lib.rs
print("Fixing lib.rs...")

with open('crates/grey_lang/src/lib.rs', 'r') as f:
    lib_content = f.read()

# Fix 3: Remove unused DiagnosticError import
lib_content = re.sub(r'use crate::diagnostics::\{DiagnosticError, Diagnostic\};',
                    'use crate::diagnostics::Diagnostic;',
                    lib_content)

with open('crates/grey_lang/src/lib.rs', 'w') as f:
    f.write(lib_content)

# Fix lexer.rs
print("Fixing lexer.rs...")

with open('crates/grey_lang/src/lexer.rs', 'r') as f:
    lexer_content = f.read()

# Fix 4: Remove unnecessary mut
lexer_content = re.sub(r'let mut chars: Vec<char> = source\.chars\(\)\.collect\(\);',
                      'let chars: Vec<char> = source.chars().collect();',
                      lexer_content)

# Fix 5: Remove duplicate '<' pattern (line 298)
# Find the duplicate pattern and remove it
lines = lexer_content.split('\n')
fixed_lines = []
skip_next = False

for i, line in enumerate(lines):
    if i == 297:  # Line 298 in 1-indexed (0-indexed is 297)
        # Check if this is the duplicate '<' pattern
        if "'<' => {" in line and i > 160:  # Skip the first one around line 164
            print(f"Removing duplicate '<' pattern at line {i+1}")
            skip_next = True
            continue
    elif skip_next and line.strip() == "":
        skip_next = False
        continue
    elif skip_next:
        continue
    
    fixed_lines.append(line)

lexer_content = '\n'.join(fixed_lines)

with open('crates/grey_lang/src/lexer.rs', 'w') as f:
    f.write(lexer_content)

print("All fixes applied!")