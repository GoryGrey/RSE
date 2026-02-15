#!/usr/bin/env python3

# Script to fix grey_compiler build issues correctly

import re

# Fix types.rs
print("Fixing types.rs...")

with open('crates/grey_lang/src/types.rs', 'r') as f:
    lines = f.readlines()

# Fix 1: Remove unused DiagnosticError import
for i, line in enumerate(lines):
    if line.strip() == 'use crate::diagnostics::{Diagnostic, DiagnosticError};':
        lines[i] = 'use crate::diagnostics::Diagnostic;\n'
        print(f"Fixed unused import at line {i+1}")

# Fix 2: Add & to param.param_type (convert to reference)
for i, line in enumerate(lines):
    if 'self.convert_ast_type(param.param_type)?;' in line:
        lines[i] = line.replace('param.param_type', 'param.param_type')
        # The above keeps it the same, we need to add & manually
        # Let's do a proper replacement
        lines[i] = re.sub(r'self\.convert_ast_type\(param\.param_type\)\?;',
                         'self.convert_ast_type(¶m.param_type)?;', 
                         line)
        print(f"Fixed type reference at line {i+1}")

with open('crates/grey_lang/src/types.rs', 'w') as f:
    f.writelines(lines)

# Fix lib.rs
print("Fixing lib.rs...")

with open('crates/grey_lang/src/lib.rs', 'r') as f:
    lines = f.readlines()

# Fix 3: Remove unused DiagnosticError import
for i, line in enumerate(lines):
    if line.strip() == 'use crate::diagnostics::{DiagnosticError, Diagnostic};':
        lines[i] = 'use crate::diagnostics::Diagnostic;\n'
        print(f"Fixed unused import at line {i+1}")

with open('crates/grey_lang/src/lib.rs', 'w') as f:
    f.writelines(lines)

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
# Find and remove the second '<' pattern that creates LessThan token
new_lines = []
skip_lines = 0

for i, line in enumerate(lines):
    if skip_lines > 0:
        skip_lines -= 1
        continue
        
    # Check for the duplicate '<' pattern that creates LessThan token
    # This should be around line 298 (0-indexed 297)
    if i == 297 and "'<' => {" in line:
        # Skip this duplicate pattern (7 lines total)
        print(f"Removing duplicate '<' pattern starting at line {i+1}")
        skip_lines = 6  # Skip the next 6 lines to remove the entire pattern
        continue
    
    new_lines.append(line)

with open('crates/grey_lang/src/lexer.rs', 'w') as f:
    f.writelines(new_lines)

print("All fixes applied!")