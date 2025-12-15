#!/usr/bin/env python3

# Script to fix grey_compiler build issues correctly

import re

# Fix types.rs
print("Fixing types.rs...")

with open('crates/grey_lang/src/types.rs', 'r') as f:
    content = f.read()

# Fix 1: Remove unused DiagnosticError import
content = content.replace('use crate::diagnostics::{Diagnostic, DiagnosticError};', 
                         'use crate::diagnostics::Diagnostic;')

# Fix 2: Add & to param.param_type 
content = content.replace('self.convert_ast_type(param.param_type)?;',
                         'self.convert_ast_type(¶m.param_type)?;')

with open('crates/grey_lang/src/types.rs', 'w') as f:
    f.write(content)

print("Fixed types.rs")

# Fix lib.rs
print("Fixing lib.rs...")

with open('crates/grey_lang/src/lib.rs', 'r') as f:
    content = f.read()

# Fix 3: Remove unused DiagnosticError import
content = content.replace('use crate::diagnostics::{DiagnosticError, Diagnostic};',
                         'use crate::diagnostics::Diagnostic;')

with open('crates/grey_lang/src/lib.rs', 'w') as f:
    f.write(content)

print("Fixed lib.rs")

# Fix lexer.rs
print("Fixing lexer.rs...")

with open('crates/grey_lang/src/lexer.rs', 'r') as f:
    lines = f.readlines()

# Fix 4: Remove unnecessary mut
for i, line in enumerate(lines):
    if 'let mut chars: Vec<char> = source.chars().collect();' in line:
        lines[i] = line.replace('let mut chars', 'let chars')
        print(f"Fixed unnecessary mut at line {i+1}")

# Fix 5: Remove duplicate '<' pattern (line 298)
new_lines = []
skip_until_line = -1

for i, line in enumerate(lines):
    # Skip lines that are part of the duplicate '<' pattern
    if skip_until_line > i:
        continue
    
    # Check for the start of duplicate LessThan pattern (around line 298)
    if i == 297 and "'<' => {" in line:
        # Skip this pattern - it's 7 lines total including the closing brace
        print(f"Removing duplicate '<' LessThan pattern starting at line {i+1}")
        skip_until_line = i + 7  # Skip 7 lines total
        continue
    
    new_lines.append(line)

with open('crates/grey_lang/src/lexer.rs', 'w') as f:
    f.writelines(new_lines)

print("Fixed lexer.rs")
print("All fixes applied!")