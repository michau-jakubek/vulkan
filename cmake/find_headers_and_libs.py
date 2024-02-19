import os
import sys

def compare_strings(a, b, ignore_case):
	result = False
	if ignore_case:
		result = a.upper() == b.upper()
	else: result = a == b
	return result

def glob_libraries(lib, path, ignore_case):
	libraries = []
	for root, dirs, files in os.walk(path):
		for lib_file in files:
			if compare_strings(lib_file, lib, ignore_case):
				libraries.append(os.path.join(root, lib_file))
	return libraries
	
def make_path(dir_list):
	path = ""
	for cat in dir_list:
		path = os.path.join(path, cat)
	return path

def path_to_list(path, upper_case):
	chunks = []
	head, tail = os.path.split(os.path.realpath(path))
	while tail:
		chunks.append(tail.upper() if upper_case else tail)
		head, tail = os.path.split(head)
	chunks.append(head)
	chunks.reverse()
	return chunks

def path_trunc_to_subpath(path, subpath, ignore_case):
	j = 0
	chunks = path_to_list(path, False)
	for i in range(len(chunks)):
		if compare_strings(chunks[i], subpath, ignore_case):
			j = i
			break
	return make_path(chunks[:(j+1)])

def path_contains(path, subdirs, ignore_case):
	if not subdirs or not subdirs[0]: return True
	if ignore_case: subdirs = [cat.upper() for cat in subdirs]
	path_list = path_to_list(path, ignore_case)
	#print("path_contains::subdirs:", subdirs)
	#print("path_contains::path_list:", path_list)
	for cat in subdirs:
		if cat not in path_list:
			return False
	return True

def find_libraries(lib, path, subdirs, ignore_case):
	result = []
	libraries = glob_libraries(lib, path, ignore_case)
	#print("find_libraries::libraries:", libraries)
	#print("find_libraries::subdirs:", subdirs)
	for lib_path in libraries:
		if path_contains(lib_path, subdirs, ignore_case):
			result.append(lib_path)
	return result

def find_header(hdr, lib_path, ignore_case):
	dirs = path_to_list(lib_path, ignore_case)
	while dirs and len(dirs) >= 2:
		headers = glob_libraries(hdr, make_path(dirs), ignore_case)
		if headers and len(headers) == 1:
			return headers[0]
		dirs.pop()
	return "0"

def find_headers_and_library(hdr_name, lib_name, look_dir, comma_hint_subdirs, ignore_case):
	subdirs = comma_hint_subdirs.split(',') if comma_hint_subdirs and repr(comma_hint_subdirs) else []
	libraries = find_libraries(lib_name, look_dir, subdirs, ignore_case)
	if len(libraries) == 1:
		headers = find_header(hdr_name, libraries[0], ignore_case)
		if repr(headers):
			return (path_trunc_to_subpath(headers, "include", True), libraries[0])
	return ("0", "{}".format(len(libraries)))

hdr_name = sys.argv[1] if len(sys.argv) >= 2 else str()
lib_name = sys.argv[2] if len(sys.argv) >= 3 else str()
look_dir = sys.argv[3] if len(sys.argv) >= 4 else str()
hint_dirs = sys.argv[4] if len(sys.argv) >= 5 else str()
ignore_case = (1 == int(sys.argv[5])) if len(sys.argv) >= 6 else False

#print("hdr_name:", hdr_name, file=sys.stderr)
#print("lib_name:", lib_name, file=sys.stderr)
#print("look_dir:", look_dir, file=sys.stderr)
#print("hint_dirs:", hint_dirs, file=sys.stderr)
#print("ignore_case:", ignore_case, file=sys.stderr)

out_hdr, out_lib = find_headers_and_library(hdr_name, lib_name, look_dir, hint_dirs, ignore_case)
print("\"{}\";\"{}\";unused_list_element".format(out_hdr, out_lib))
#print("xxx;yyy")
