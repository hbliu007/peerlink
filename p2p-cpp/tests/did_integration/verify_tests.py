#!/usr/bin/env python3
"""
Verify DID integration test structure
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
        'scenarios/test_device_registration.py',
        'scenarios/test_device_lifecycle.py',
        'scenarios/test_multi_device.py',
        'scenarios/test_error_handling.py',
        'scenarios/test_concurrent.py',
        'utils/did_client.py'
    ]

    print("\nChecking required files:")
    for file in required_files:
        filepath = os.path.join(test_dir, file)
        if os.path.exists(filepath):
            print(f"✓ {file}")
        else:
            print(f"✗ {file} - MISSING")
            errors.append(file)

    # Count tests
    test_count = 0
    for root, dirs, files in os.walk(os.path.join(test_dir, 'scenarios')):
        for file in files:
            if file.startswith('test_') and file.endswith('.py'):
                filepath = os.path.join(root, file)
                with open(filepath, 'r') as f:
                    content = f.read()
                    test_count += content.count('def test_')

    print(f"\nTotal test files: {len([f for f in required_files if f.startswith('scenarios/')])}")
    print(f"Total test functions: {test_count}")

    if errors:
        print(f"\n❌ {len(errors)} errors found")
        return 1
    else:
        print("\n✅ All checks passed!")
        return 0

if __name__ == '__main__':
    sys.exit(main())
