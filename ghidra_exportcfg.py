# Usage: GHIDRA_INSTALL_DIR="<Ghidra Directory>" python3 ghidra_exportcfg.py ...
import argparse
import os
import sys
import subprocess
import tempfile
import shutil 

GHIDRA_SCRIPTS_DIR = './ghidra_scripts'
SCRIPT_NAME = 'ExportCFG.py'

def main():
    ghidra_install_dir = os.getenv('GHIDRA_INSTALL_DIR')
    if ghidra_install_dir is None:
        print('GHIDRA_INSTALL_DIR environment variable not set')
        sys.exit(1)

    parser = argparse.ArgumentParser()
    parser.add_argument('program_filename')
    parser.add_argument('output_filename')

    args = parser.parse_args()

    program_filename = args.program_filename
    if not os.path.isfile(program_filename):
        print(f'Program file does not exist - "{program_filename}"')
        sys.exit(1)

    output_filename = args.output_filename
    if os.path.exists(output_filename):
        print(f'File or folder with the same name as output already exists - \"{output_filename}\"')
        sys.exit(1)

    fd, path = tempfile.mkstemp()
    os.close(fd)
    os.remove(path)

    tempdir_path = tempfile.mkdtemp()
    
    args = [
        f'{os.path.expanduser(ghidra_install_dir)}/support/analyzeHeadless',
        tempdir_path,
        os.path.basename(path),
        '-import',
        program_filename,
        '-loader',
        'ElfLoader',
        '-loader-loadSystemLibraries',
        'true',
        '-scriptPath',
        GHIDRA_SCRIPTS_DIR,
        '-postScript',
        SCRIPT_NAME,
        output_filename,
        '-deleteProject'
    ]

    subprocess.run(args)

    shutil.rmtree(tempdir_path)

if __name__ == '__main__':
    main()
