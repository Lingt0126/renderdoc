#!/usr/bin/env python3
"""Build the Windows compatibility variant without changing the normal source/build tree.

The checked-in patch is intentionally source-level and fail-closed: upstream changes that
invalidate it require review, rather than a global text replacement during every build.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime, timezone


REPO = Path(__file__).resolve().parents[2]
PATCH = Path(__file__).with_name('stage1-identity.patch')
MARKER = '.renderdoc-compat-source.json'


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run(args, cwd):
    subprocess.run([str(arg) for arg in args], cwd=cwd, check=True)


def git(*args):
    return subprocess.check_output(['git', *args], cwd=REPO)


def prepare(build_root):
    source = build_root / 'source'
    if source.exists() and not (source / MARKER).is_file():
        raise RuntimeError(f'Refusing to overwrite an unowned source directory: {source}')
    source.mkdir(parents=True, exist_ok=True)
    inputs = {}
    # Includes local tracked edits, never resets/stashes them. Unrelated untracked files stay out.
    names = git('ls-files', '-z').decode('utf-8').split('\0')
    for name in names:
        if not name:
            continue
        origin = REPO / name
        if not origin.is_file():
            raise RuntimeError(f'Tracked input missing: {origin}')
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(origin, target)
        inputs[name] = sha256(origin)
    # The locally supplied PySide SDK contains ignored .lib/.dll dependencies.
    pyside = REPO / 'qrenderdoc/3rdparty/pyside'
    if pyside.is_dir():
        shutil.copytree(pyside, source / pyside.relative_to(REPO), dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    # A previous generated props file is ours; other source files are restored from inputs above.
    props = source / 'Directory.Build.props'
    if 'Directory.Build.props' not in inputs and props.exists():
        props.unlink()
    run(['git', 'apply', '--check', '--whitespace=nowarn', PATCH], source)
    run(['git', 'apply', '--whitespace=nowarn', PATCH], source)
    metadata = {
        'source_commit': git('rev-parse', 'HEAD').decode().strip(),
        'source_dirty': git('status', '--porcelain', '--untracked-files=no').decode().splitlines(),
        'patch_sha256': sha256(PATCH),
        'prepared_utc': datetime.now(timezone.utc).isoformat(),
        'input_sha256': inputs,
    }
    (source / MARKER).write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    print(f'Prepared {len(inputs)} tracked inputs at {source}', flush=True)
    return source, metadata


def validate_contracts(source):
    import re
    # Applications built against unchanged public headers must still identify a replay host.
    for name in ('renderdoc/api/app/renderdoc_app.h', 'renderdoc/api/replay/renderdoc_replay.h',
                 'qrenderdoc/Code/pyrenderdoc/renderdoc.i', 'qrenderdoc/Code/pyrenderdoc/qrenderdoc.i'):
        if sha256(source / name) != sha256(REPO / name):
            raise RuntimeError(f'Public interface changed unexpectedly: {name}')
    for name in ('renderdoc/os/win32/win32_libentry.cpp', 'renderdoc/os/posix/posix_libentry.cpp'):
        if 'LibraryHooks::Detect("renderdoc__replay__marker")' not in (source / name).read_text():
            raise RuntimeError(f'Public replay marker not accepted: {name}')
    shader_entries = set()
    shader_symbols = set()
    for folder in ('hlsl', 'glsl'):
        for item in (source / 'renderdoc/data' / folder).iterdir():
            if not item.is_file():
                continue
            text = item.read_text(encoding='utf-8-sig')
            shader_entries.update(re.findall(r'\bRENDERTEST_\w+\s*(?=\()', text))
            shader_symbols.update(re.findall(r'\bRENDERTEST_\w+', text))
            if re.search(r'\bRENDERDOC_\w+\s*\(', text):
                raise RuntimeError(f'Unmigrated shader entry in {item}')
    shader_entries = {entry.strip() for entry in shader_entries}
    for folder in ('d3d11', 'd3d12', 'gl'):
        for item in (source / 'renderdoc/driver' / folder).rglob('*.cpp'):
            for entry in re.findall(r'"(RENDERTEST_\w+)"', item.read_text(encoding='utf-8-sig')):
                if entry not in shader_symbols:
                    raise RuntimeError(f'Shader query without matching definition: {entry} in {item}')
    app_api = (source / 'renderdoc/replay/entry_points.cpp').read_text(encoding='utf-8-sig')
    if '"RENDERTEST_GetAPI"' in app_api:
        raise RuntimeError('Self-host lookup violates the public RENDERDOC_GetAPI contract')
    print(f'Public headers, replay marker and {len(shader_entries)} shader-entry contracts passed', flush=True)


def msbuild_path():
    for edition in ('Community', 'Professional', 'Enterprise', 'BuildTools'):
        path = Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / (
            f'Microsoft Visual Studio/2022/{edition}/MSBuild/Current/Bin/MSBuild.exe')
        if path.is_file():
            return path
    raise RuntimeError('Visual Studio 2022 MSBuild not found')


def build(source, metadata, build_root, platforms, jobs):
    for platform in platforms:
        log = build_root / f'build-{platform}.log'
        run([msbuild_path(), 'renderdoc.sln', '/t:Build', '/p:Configuration=Development',
             f'/p:Platform={platform}', f'/p:CommitId={metadata["source_commit"]}',
             '/p:LinkIncremental=false', f'/m:{jobs}', '/nologo', '/verbosity:quiet',
             '/fl', f'/flp:logfile={log};verbosity=normal'], source)
        print(f'Built {platform}: {log}', flush=True)


def deploy(source, metadata, destination):
    # This tool deliberately cannot overwrite the existing standard installation.
    if destination.resolve() != Path(r'C:\RenderDocCompat').resolve():
        raise RuntimeError('Compatibility deployment is restricted to C:\\RenderDocCompat')
    if destination.exists() and not (destination / 'compat-build.json').is_file():
        if any(destination.iterdir()):
            raise RuntimeError('Nonempty deployment without compat-build.json; review it first')
    outputs = [('x64', source / 'x64/Development', destination),
               ('x86', source / 'Win32/Development', destination / 'x86')]
    for arch, origin, target in outputs:
        for required in ('rendertest.dll', 'rendertestcmd.exe', 'qrendertest.exe'):
            if not (origin / required).is_file():
                raise RuntimeError(f'Missing {arch} output: {origin / required}')
        if (origin / 'renderdoc.dll').exists() or (origin / 'rdoc.dll').exists():
            raise RuntimeError(f'Mixed normal/compatibility output in {origin}')
    # Detect a live GUI/game before changing any deployed file, not halfway through copying.
    if os.name == 'nt' and destination.exists():
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        create_file = kernel.CreateFileW
        create_file.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD,
                                wintypes.HANDLE]
        create_file.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        for item in destination.rglob('*'):
            if not item.is_file():
                continue
            handle = create_file(str(item), 0xC0000000, 0, None, 3, 0x80, None)
            if handle == wintypes.HANDLE(-1).value:
                raise RuntimeError(f'Deployment file is in use or not writable: {item} '
                                   f'(Windows error {ctypes.get_last_error()})')
            kernel.CloseHandle(handle)
    destination.mkdir(parents=True, exist_ok=True)
    # Preserve the preceding staged deliverable before updating any file; no destructive mirror.
    if (destination / 'compat-build.json').is_file():
        backup = destination.parent / ('RenderDocCompat-backup-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
        shutil.copytree(destination, backup)
    hashes = {}
    for arch, origin, target in outputs:
        for item in origin.rglob('*'):
            rel = item.relative_to(origin)
            if 'obj' in rel.parts or item.suffix in ('.lib', '.exp', '.ilk') or not item.is_file():
                continue
            dest = target / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, dest)
            digest = sha256(item)
            if sha256(dest) != digest:
                raise RuntimeError(f'Deployment hash mismatch: {dest}')
            hashes[str(dest.relative_to(destination))] = digest
    for license_name in ('LICENSE.md',):
        shutil.copy2(REPO / license_name, destination / license_name)
    manifest = {key: value for key, value in metadata.items() if key != 'input_sha256'}
    manifest.update({'deployed_utc': datetime.now(timezone.utc).isoformat(), 'sha256': hashes})
    (destination / 'compat-build.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(f'Deployed and verified {len(hashes)} files at {destination}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-root', type=Path, required=True)
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--reuse-source', action='store_true', help='Resume the same prepared snapshot')
    parser.add_argument('--platform', choices=['x64', 'x86'], action='append')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--deploy', action='store_true')
    args = parser.parse_args()
    build_root = args.build_root.resolve()
    if build_root == REPO or REPO in build_root.parents:
        raise RuntimeError('Build root must be outside the canonical repository')
    build_root.mkdir(parents=True, exist_ok=True)
    if args.reuse_source:
        source = build_root / 'source'
        metadata = json.loads((source / MARKER).read_text(encoding='utf-8'))
        if metadata['patch_sha256'] != sha256(PATCH):
            raise RuntimeError('Patch changed; prepare the source again before building')
    else:
        source, metadata = prepare(build_root)
    validate_contracts(source)
    if not args.prepare_only:
        build(source, metadata, build_root, args.platform or ['x64', 'x86'], args.jobs)
    if args.deploy:
        deploy(source, metadata, Path(r'C:\RenderDocCompat'))


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
