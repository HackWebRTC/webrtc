import sys
import glob
import subprocess

src_black_list_keywords = [
    'googletest', 'gtest', 'mock', '_unittest', '_test', '_integrationtest', '_benchmark'
]
src_white_list = [
    'rtc_base/gtest_prod_util.h'
]
special_related_src = {
    'common_audio/include/audio_util.h': [
        'common_audio/audio_util.cc',
    ],
    'system_wrappers/include/metrics.h': [
        # 'system_wrappers/include/metrics_default.h',
        # header won't be included in `find_sources`, 
        # but will be included in `extract_includes`
        'system_wrappers/source/metrics_default.cc',
    ],
    'third_party/abseil-cpp/absl/strings/string_view.cc': [
        'third_party/abseil-cpp/absl/strings/internal/memutil.cc',
    ],
}

def file_not_blacklisted(file):
    for keyword in src_black_list_keywords:
        if keyword in file:
            discard = True
            for white_list_file in src_white_list:
                if white_list_file in file:
                    discard = False
            if discard:
                return False
    return True

def find_full_path(src_set, file):
    for src in src_set:
        if src.endswith(file):
            return src
    return None

def find_sources(all_src, headers):
    sources = []
    for header in headers:
        name = '.'.join(header.split('.')[:-1])
        for src in all_src:
            if src.startswith(name) and file_not_blacklisted(src):
                sources.append(src)
                break
        for special_header, related_srcs in special_related_src.items():
            if header.endswith(special_header):
                for related_src in related_srcs:
                    full_path = find_full_path(all_src, related_src)
                    if full_path is not None:
                        sources.append(full_path)
    return sources

def get_full_path(headers, sources, file):
    header = find_full_path(headers, file)
    if header is not None:
        return header
    source = find_full_path(sources, file)
    if source is not None:
        return source
    return None

def extract_includes(headers, file):
    with open(file, 'r') as f:
        includes = [line for line in f.read().split('\n') if line.startswith('#include') and '\"' in line]
        includes = [line.split('\"')[1] for line in includes]

        results = []
        for include in includes:
            full_path = get_full_path(headers, [], include)
            if full_path is not None and file_not_blacklisted(full_path):
                results.append(full_path)
        return results

def find_all_related_files(headers, sources, wanted):
    # 对 candidates 中的每个 file，读出直接相关的头文件和源文件 related，把 file 加入到 searched 中，
    # 同时对于 related 里不在 searched 中的项目，加入到 new_candidates 中，
    # candidates 遍历结束后，把 new_candidates 赋值给 candidates，继续遍历，
    # 直到 candidates 为空。
    # candidates 初始化为 wanted。
    # searched 就是所求。

    searched = set()
    candidates = []
    candidates.extend(wanted)
    while len(candidates) != 0:
        new_candidates = []
        for file in candidates:
            full_path = get_full_path(headers, sources, file)
            if full_path is None:
                continue

            searched.add(full_path)
            related_headers = extract_includes(headers, full_path)
            related_sources = find_sources(sources, related_headers)

            new_candidates.extend([f for f in related_headers if f not in searched])
            new_candidates.extend([f for f in related_sources if f not in searched])

        candidates = new_candidates

        print('searched', len(searched))
        print('candidates', len(candidates))
    return sorted(searched)

def get_parents(repo, file):
    parts = file[len(repo):].split('/')
    return parts[:-1]

def copy_to(repo, file, dst):
    parents = get_parents(repo, file)
    dir = '%s/%s' % (dst, '/'.join(parents))
    subprocess.run('mkdir -p %s' % dir, shell=True, check=True)
    subprocess.run('cp %s %s' % (file, dir), shell=True, check=True)

if __name__ == '__main__':
    repo = sys.argv[1]
    dst = sys.argv[2]
    wanted = sys.argv[3:]

    all_files = glob.glob('%s/**/*' % repo, recursive=True)
    headers = [f for f in all_files if f.endswith('.h') or f.endswith('.hpp')]
    sources = [f for f in all_files if f.endswith('.c') or f.endswith('.cc') or f.endswith('.cpp')]
    all_files = None

    print('wanted', wanted)
    print('header', len(headers))
    print('source', len(sources))

    needed = find_all_related_files(headers, sources, wanted)

    for file in needed:
        copy_to(repo, file, dst)
