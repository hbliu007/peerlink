#!/usr/bin/env python3
"""
Verify system test structure and syntax
"""
import os
import sys
import ast

def check_file(filepath):
    """Check if Python file has valid syntax"""
    try:
        with open(filepath, 'r') as f:
            ast.parse(f.read())
        return True, None
    except SyntaxError as e:
        return False, str(e)

def main():
    test_dir = os.path.dirname(os.path.abspath(__file__))
    errors = []

    # Check all Python files
    for root, dirs, files in os.walk(test_dir):
        for file in files:
            if file.endswith('.py'):
                filepath = os.path.join(root, file)
                valid, error = check_file(filepath)

                if valid:
                    print(f"✓ {os.path.relpath(filepath, test_dir)}")
                else:
                    print(f"✗ {os.path.relpath(filepath, test_dir)}: {error}")
                    errors.append(filepath)

    # Check required files
    required_files = [
        'requirements.txt',
        'conftest.py',
        'run_tests.sh',
        'README.md',
        'scenarios/test_p2p_connection.py',
        'scenarios/test_relay_fallback.py',
        'scenarios/test_fault_recovery.py',
        'scenarios/test_performance.py',
        'utils/test_clients.py'
    ]

    print("\nChecking required files:")
    for file in required_files:
        filepath = os.path.join(test_dir, file)
        if os.path.exists(filepath):
            print(f"✓ {file}")
        else:
            print(f"✗ {file} - MISSING")
            errors.append(file)

    print(f"\nTotal files checked: {len(required_files)}")

    if errors:
        print(f"\n❌ {len(errors)} errors found")
        return 1
    else:
        print("\n✅ All checks passed!")
        return 0

if __name__ == '__main__':
    sys.exit(main())
