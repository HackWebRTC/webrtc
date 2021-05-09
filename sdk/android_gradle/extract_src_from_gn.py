import sys

def output(lines, start, end, prefix):
    index = start
    while index <= end:
        line = lines[index]
        splitter = None
        if '\'' in line:
            splitter = '\''
        elif '\"' in line:
            splitter = '\"'
        if splitter is not None:
            path = line.split(splitter)[1]
            if not path.endswith('.h'):
                if path.startswith('//'):
                    print(prefix + path[2:])
                else:
                    print(prefix + path)
        index = index + 1

def extract(gn_path, gn_src_set, output_prefix):
    with open(gn_path, 'r') as f:
        content = f.read().split('\n')
    start_line = -1
    end_line = -1
    index = 0
    all_lines = len(content)
    while index < all_lines:
        if content[index].endswith('%s = [' % gn_src_set):
            start_line = index
        if start_line != -1 and content[index].endswith(']'):
            end_line = index
            break
        index = index + 1
    if start_line != -1 and end_line != -1:
        output(content, start_line + 1, end_line - 1, output_prefix)

if __name__ == '__main__':
    gn_path = sys.argv[1]
    gn_src_set = sys.argv[2]
    output_prefix = sys.argv[3]
    extract(gn_path, gn_src_set, output_prefix)
