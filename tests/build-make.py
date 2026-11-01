#!/usr/bin/env python3
"""Build, stage and clean with either BSD make or GNU make, without c99."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--make', default='make', help='make command, including optional arguments')
parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
args = parser.parse_args()
source = args.project.resolve()
terminal = (source / 'st.c').is_file()
compiler = shutil.which('cc')
assert compiler, 'cc is required'
make = shlex.split(args.make)

with tempfile.TemporaryDirectory(prefix='make-portability.') as temp:
    scratch = Path(temp)
    project = scratch / 'source'
    generated = {'.git', 'config.h', 'dependency', 'build', '__pycache__',
                 'st', 'st-scrollback-viewer', 'vis', 'vis-menu', 'vis-digraph',
                 'vis-single', 'vis-single-payload.inc'}
    if not terminal:
        generated.add('config.mk')

    def ignored(directory, names):
        return [name for name in names if name in {'.git', 'dependency', 'build', '__pycache__'}
                or (Path(directory) == source and name in generated)
                or name.endswith(('.o', '.pyc', '.out', '.err'))]

    shutil.copytree(source, project, ignore=ignored, symlinks=True)
    tools = scratch / 'bin'
    tools.mkdir()
    log = scratch / 'compiler.log'
    env = os.environ.copy()
    for key in ('CC', 'CFLAGS', 'CPPFLAGS', 'LDFLAGS', 'MAKEFLAGS', 'MFLAGS', 'MAKELEVEL'):
        env.pop(key, None)
    env.update(PATH=str(tools) + ':' + env['PATH'], BUILD_COMPILER_LOG=str(log))
    for name in ('cc', 'cc-override'):
        wrapper = tools / name
        wrapper.write_text('#!/bin/sh\nprintf "%s\\n" "$0 $*" >>"$BUILD_COMPILER_LOG"\nexec '
                           + shlex.quote(compiler) + ' "$@"\n')
        wrapper.chmod(0o700)
    unavailable = tools / 'c99'
    unavailable.write_text('#!/bin/sh\necho "FAIL: c99 must not be used" >&2\nexit 127\n')
    unavailable.chmod(0o700)
    stage = scratch / 'stage'
    terminfo = stage / 'share' / 'terminfo'
    terminfo.mkdir(parents=True)
    env['TERMINFO'] = str(terminfo)

    def run(command, cwd=project):
        result = subprocess.run(command, cwd=cwd, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise AssertionError(shlex.join(command) + '\n' + result.stdout[-8000:])
        return result.stdout

    # clean must parse even before vis has generated config.mk.
    run(make + ['clean'])
    if not terminal:
        run(make + ['config.mk'])
        run(['./configure', '--enable-lua', '--disable-lpeg-static'])
    run(make)
    binaries = ('st', 'st-scrollback-viewer') if terminal else ('vis', 'vis-menu', 'vis-digraph')
    for binary in binaries:
        assert os.access(project / binary, os.X_OK), binary
    assert '-std=c99' in log.read_text(), 'C99 language mode was not selected'
    if not terminal:
        assert '+lua' in run(['./vis', '-v'])

    run(make + ['install', 'DESTDIR=' + str(stage)])
    for binary in binaries:
        assert os.access(stage / 'usr/local/bin' / binary, os.X_OK), binary
    if not terminal:
        assert (stage / 'usr/local/share/vis/vis-scrollback.lua').is_file()
    run(make + ['uninstall', 'DESTDIR=' + str(stage)])
    for binary in binaries:
        assert not (stage / 'usr/local/bin' / binary).exists(), binary

    if terminal:
        run(make + ['dist'])
        archive = next(project.glob('st-*.tar.gz'))
        exported = scratch / 'archive'
        shutil.unpack_archive(archive, exported)
        unpacked = next(exported.iterdir())
        assert (unpacked / 'build.mk').is_file()
        run(make, cwd=unpacked)

    run(make + ['clean'])
    for binary in binaries:
        assert not (project / binary).exists(), binary
    log.write_text('')
    run(make + ['CC=' + str(tools / 'cc-override'), 'CFLAGS=-O0 -DMAKE_PORTABILITY_CHECK=1'])
    calls = log.read_text()
    assert 'cc-override' in calls and '-DMAKE_PORTABILITY_CHECK=1' in calls
    if not terminal:
        run(make + ['distclean'])
        assert not (project / 'config.mk').exists()
    print(('st' if terminal else 'vis') + ': ' + shlex.join(make) +
          ' passed clean build, no c99, staged install/uninstall and CC/CFLAGS overrides')
